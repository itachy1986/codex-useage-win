#include "LocalUsageReader.h"

#include "JsonLite.h"

#include <Windows.h>

#include <algorithm>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <vector>

namespace {

struct Date { int year = 0; int month = 0; int day = 0; };
struct Event { Date date; std::wstring model; TokenUsage total; std::optional<TokenUsage> last; };
struct Session { std::filesystem::file_time_type modified; std::vector<Event> events; };

std::wstring Wide(const std::string_view text) {
    if (text.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring output(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), output.data(), count);
    return output;
}

const jsonlite::Value* FindDescendant(const jsonlite::Value& value, std::string_view key) {
    if (value.IsObject()) {
        if (const auto* direct = value.Find(key)) return direct;
        for (const auto& [_, child] : *value.AsObject()) if (const auto* found = FindDescendant(child, key)) return found;
    } else if (value.IsArray()) {
        for (const auto& child : *value.AsArray()) if (const auto* found = FindDescendant(child, key)) return found;
    }
    return nullptr;
}

long long Number(const jsonlite::Value* node, std::string_view key) {
    if (!node) return 0;
    const auto* field = node->Find(key);
    const auto value = field ? field->AsNumber() : std::nullopt;
    return value ? std::max(0LL, static_cast<long long>(*value)) : 0;
}

std::optional<TokenUsage> UsageFrom(const jsonlite::Value* node) {
    if (!node || !node->IsObject()) return std::nullopt;
    const auto* total = node->Find("total_tokens");
    if (!total || !total->AsNumber()) return std::nullopt;
    TokenUsage usage;
    usage.inputTokens = Number(node, "input_tokens");
    usage.cachedInputTokens = Number(node, "cached_input_tokens");
    usage.cacheWriteInputTokens = Number(node, "cache_write_input_tokens");
    usage.outputTokens = Number(node, "output_tokens");
    usage.totalTokens = Number(node, "total_tokens");
    return usage;
}

Date ParseDate(const jsonlite::Value& line, std::optional<int> localUtcOffsetMinutesForTesting) {
    const auto* stamp = line.Find("timestamp");
    const auto text = stamp ? stamp->AsString() : std::nullopt;
    Date date;
    if (!text || text->size() < 19) return date;
    try {
        SYSTEMTIME parsed{};
        parsed.wYear = static_cast<WORD>(std::stoi(std::string(text->substr(0, 4))));
        parsed.wMonth = static_cast<WORD>(std::stoi(std::string(text->substr(5, 2))));
        parsed.wDay = static_cast<WORD>(std::stoi(std::string(text->substr(8, 2))));
        parsed.wHour = static_cast<WORD>(std::stoi(std::string(text->substr(11, 2))));
        parsed.wMinute = static_cast<WORD>(std::stoi(std::string(text->substr(14, 2))));
        parsed.wSecond = static_cast<WORD>(std::stoi(std::string(text->substr(17, 2))));
        size_t timezone = text->find_first_of("Zz+-", 19);
        int offsetMinutes = 0;
        if (timezone != std::string_view::npos && (*text)[timezone] != 'Z' && (*text)[timezone] != 'z') {
            const int sign = (*text)[timezone] == '+' ? 1 : -1;
            offsetMinutes = sign * (std::stoi(std::string(text->substr(timezone + 1, 2))) * 60
                + std::stoi(std::string(text->substr(timezone + 4, 2))));
        }
        FILETIME raw{};
        if (!SystemTimeToFileTime(&parsed, &raw)) return date;
        ULARGE_INTEGER value{}; value.LowPart = raw.dwLowDateTime; value.HighPart = raw.dwHighDateTime;
        value.QuadPart -= static_cast<LONGLONG>(offsetMinutes) * 60LL * 10000000LL;
        FILETIME utc{}; utc.dwLowDateTime = value.LowPart; utc.dwHighDateTime = value.HighPart;
        SYSTEMTIME utcTime{}; SYSTEMTIME local{};
        if (!FileTimeToSystemTime(&utc, &utcTime)) return date;
        if (localUtcOffsetMinutesForTesting.has_value()) {
            value.QuadPart += static_cast<LONGLONG>(*localUtcOffsetMinutesForTesting) * 60LL * 10000000LL;
            FILETIME forced{}; forced.dwLowDateTime = value.LowPart; forced.dwHighDateTime = value.HighPart;
            if (!FileTimeToSystemTime(&forced, &local)) return date;
        } else if (!SystemTimeToTzSpecificLocalTime(nullptr, &utcTime, &local)) {
            return date;
        }
        date.year = local.wYear; date.month = local.wMonth; date.day = local.wDay;
    } catch (...) {}
    return date;
}

bool SameDate(const Date& a, const Date& b) { return a.year == b.year && a.month == b.month && a.day == b.day; }
bool IsAtLeast(const TokenUsage& next, const TokenUsage& previous) {
    return next.inputTokens >= previous.inputTokens && next.cachedInputTokens >= previous.cachedInputTokens
        && next.cacheWriteInputTokens >= previous.cacheWriteInputTokens && next.outputTokens >= previous.outputTokens
        && next.totalTokens >= previous.totalTokens;
}
TokenUsage Difference(const TokenUsage& a, const TokenUsage& b) {
    return {a.inputTokens-b.inputTokens, a.cachedInputTokens-b.cachedInputTokens, a.cacheWriteInputTokens-b.cacheWriteInputTokens,
        a.outputTokens-b.outputTokens, a.totalTokens-b.totalTokens};
}
void Add(TokenUsage* to, const TokenUsage& from) {
    to->inputTokens += from.inputTokens; to->cachedInputTokens += from.cachedInputTokens; to->cacheWriteInputTokens += from.cacheWriteInputTokens;
    to->outputTokens += from.outputTokens; to->totalTokens += from.totalTokens;
}
void AddToScope(LocalUsageScope* scope, const TokenUsage& usage, const std::wstring& model) {
    scope->available = true; Add(&scope->usage, usage); Add(&scope->byModel[model], usage);
}

std::filesystem::path ResolveHome(std::filesystem::path home) {
    if (!home.empty()) return home;
    wchar_t buffer[32768] = {};
    const DWORD codex = GetEnvironmentVariableW(L"CODEX_HOME", buffer, static_cast<DWORD>(std::size(buffer)));
    if (codex > 0 && codex < std::size(buffer)) return buffer;
    const DWORD profile = GetEnvironmentVariableW(L"USERPROFILE", buffer, static_cast<DWORD>(std::size(buffer)));
    return profile > 0 ? std::filesystem::path(buffer) / L".codex" : std::filesystem::path();
}

}  // namespace

LocalUsageReader::LocalUsageReader(std::filesystem::path codexHome, std::optional<int> localUtcOffsetMinutesForTesting)
    : codexHome_(ResolveHome(std::move(codexHome))), localUtcOffsetMinutesForTesting_(localUtcOffsetMinutesForTesting) {}

LocalUsageSnapshot LocalUsageReader::Scan() const {
    SYSTEMTIME now{}; GetLocalTime(&now);
    return ScanForLocalDate(now.wYear, now.wMonth, now.wDay);
}

LocalUsageSnapshot LocalUsageReader::ScanForLocalDate(int year, int month, int day) const {
    LocalUsageSnapshot result;
    std::vector<Session> sessions;
    const auto tree = codexHome_ / L"sessions";
    std::error_code error;
    if (!std::filesystem::exists(tree, error)) return result;
    for (std::filesystem::recursive_directory_iterator it(tree, error), end; it != end; it.increment(error)) {
        if (error) { error.clear(); continue; }
        if (!it->is_regular_file(error) || it->path().extension() != L".jsonl") continue;
        ++result.filesScanned;
        std::ifstream file(it->path(), std::ios::binary);
        std::string line; Session session; session.modified = it->last_write_time(error); std::wstring model;
        bool malformed = false;
        while (std::getline(file, line)) {
            jsonlite::Parser parser(line); const auto parsed = parser.Parse();
            if (!parsed) { malformed = true; continue; }
            if (const auto* modelNode = FindDescendant(*parsed, "model")) if (const auto value = modelNode->AsString()) model = Wide(*value);
            const auto total = UsageFrom(FindDescendant(*parsed, "total_token_usage"));
            if (!total) continue;
            Event event; event.date = ParseDate(*parsed, localUtcOffsetMinutesForTesting_); event.model = model; event.total = *total;
            event.last = UsageFrom(FindDescendant(*parsed, "last_token_usage"));
            session.events.push_back(std::move(event));
        }
        if (malformed) ++result.filesWithParseErrors;
        if (!session.events.empty()) sessions.push_back(std::move(session));
    }
    if (sessions.empty()) return result;
    std::sort(sessions.begin(), sessions.end(), [](const Session& a, const Session& b) { return a.modified < b.modified; });
    const Date today{year, month, day};
    for (const auto& session : sessions) {
        LocalUsageScope sessionTotal;
        TokenUsage previous{}; bool havePrevious = false;
        for (const auto& event : session.events) {
            if (event.last && &session == &sessions.back()) { result.last = {}; AddToScope(&result.last, *event.last, event.model); }
            if (!havePrevious) {
                AddToScope(&sessionTotal, event.total, event.model);
                if (SameDate(event.date, today)) AddToScope(&result.today, event.total, event.model);
                previous = event.total; havePrevious = true; continue;
            }
            if (!IsAtLeast(event.total, previous)) { previous = event.total; continue; }
            const TokenUsage delta = Difference(event.total, previous);
            AddToScope(&sessionTotal, delta, event.model);
            if (SameDate(event.date, today)) AddToScope(&result.today, delta, event.model);
            previous = event.total;
        }
        result.tillNow.available = true;
        Add(&result.tillNow.usage, sessionTotal.usage);
        for (const auto& [model, usage] : sessionTotal.byModel) Add(&result.tillNow.byModel[model], usage);
        if (&session == &sessions.back()) result.task = sessionTotal;
    }
    return result;
}
