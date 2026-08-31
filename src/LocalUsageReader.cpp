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
struct Event {
    Date date;
    long long unixSeconds = 0;
    std::wstring model;
    AttributionSource attribution = AttributionSource::MissingOrExplicitUnpriced;
    TokenUsage total;
    std::optional<TokenUsage> last;
};
struct ModelAttribution {
    std::wstring model;
    AttributionSource source = AttributionSource::CanonicalMetadata;
};
struct Session { std::filesystem::file_time_type modified; std::vector<Event> events; };

std::wstring Wide(const std::string_view text) {
    if (text.empty()) return {};
    const int count = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    std::wstring output(count, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), output.data(), count);
    return output;
}

const jsonlite::Value* PayloadFor(const jsonlite::Value& line) {
    const auto* payload = line.Find("payload");
    return payload != nullptr && payload->IsObject() ? payload : nullptr;
}

std::optional<std::string_view> EventType(const jsonlite::Value& line) {
    const auto* type = PayloadFor(line) != nullptr ? PayloadFor(line)->Find("type") : nullptr;
    return type != nullptr ? type->AsString() : std::nullopt;
}

std::optional<ModelAttribution> CanonicalModelFromMetadata(const jsonlite::Value& line) {
    const auto type = EventType(line);
    const auto* payload = PayloadFor(line);
    if (!type || payload == nullptr) return std::nullopt;

    // Codex rollout metadata owns model state. Do not recursively inspect
    // arbitrary nested objects: tool payloads can also contain a "model" key.
    const jsonlite::Value* model = nullptr;
    if (*type == "turn_context") {
        model = payload->Find("model");
    } else if (*type == "thread_settings_applied") {
        const auto* settings = payload->Find("thread_settings");
        model = settings != nullptr ? settings->Find("model") : nullptr;
    }
    const auto value = model != nullptr ? model->AsString() : std::nullopt;
    if (value && !value->empty()) return ModelAttribution{Wide(*value), AttributionSource::CanonicalMetadata};

    // This is intentionally a narrow, schema-bound mapping. It is consulted only
    // from thread_settings_applied and never from arbitrary JSON strings.
    if (*type == "thread_settings_applied") {
        const auto* settings = payload->Find("thread_settings");
        const auto* feature = settings != nullptr ? settings->Find("feature") : nullptr;
        const auto featureName = feature != nullptr ? feature->AsString() : std::nullopt;
        if (featureName && *featureName == "code_review") {
            return ModelAttribution{L"gpt-5.3-codex", AttributionSource::ValidatedFeatureMapping};
        }
        if (featureName && *featureName == "auto_review") {
            return ModelAttribution{L"gpt-5.4", AttributionSource::ValidatedFeatureMapping};
        }
    }
    return std::nullopt;
}

const jsonlite::Value* TokenUsageInfo(const jsonlite::Value& line) {
    const auto type = EventType(line);
    const auto* payload = PayloadFor(line);
    if (!type || *type != "token_count" || payload == nullptr) return nullptr;
    const auto* info = payload->Find("info");
    return info != nullptr && info->IsObject() ? info : nullptr;
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

long long ParseUnixSeconds(const jsonlite::Value& line) {
    const auto* stamp = line.Find("timestamp");
    const auto text = stamp ? stamp->AsString() : std::nullopt;
    if (!text || text->size() < 19) return 0;
    try {
        SYSTEMTIME parsed{};
        parsed.wYear = static_cast<WORD>(std::stoi(std::string(text->substr(0, 4))));
        parsed.wMonth = static_cast<WORD>(std::stoi(std::string(text->substr(5, 2))));
        parsed.wDay = static_cast<WORD>(std::stoi(std::string(text->substr(8, 2))));
        parsed.wHour = static_cast<WORD>(std::stoi(std::string(text->substr(11, 2))));
        parsed.wMinute = static_cast<WORD>(std::stoi(std::string(text->substr(14, 2))));
        parsed.wSecond = static_cast<WORD>(std::stoi(std::string(text->substr(17, 2))));
        const size_t timezone = text->find_first_of("Zz+-", 19);
        int offsetMinutes = 0;
        if (timezone != std::string_view::npos && (*text)[timezone] != 'Z' && (*text)[timezone] != 'z') {
            const int sign = (*text)[timezone] == '+' ? 1 : -1;
            offsetMinutes = sign * (std::stoi(std::string(text->substr(timezone + 1, 2))) * 60
                + std::stoi(std::string(text->substr(timezone + 4, 2))));
        }
        FILETIME fileTime{};
        if (!SystemTimeToFileTime(&parsed, &fileTime)) return 0;
        ULARGE_INTEGER value{}; value.LowPart = fileTime.dwLowDateTime; value.HighPart = fileTime.dwHighDateTime;
        return static_cast<long long>(value.QuadPart / 10000000ULL) - 11644473600LL - static_cast<long long>(offsetMinutes) * 60LL;
    } catch (...) { return 0; }
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
void AddToScope(LocalUsageScope* scope, const TokenUsage& usage, const std::wstring& model,
    AttributionSource attribution, long long unixSeconds) {
    scope->available = true;
    Add(&scope->usage, usage);
    Add(&scope->byModel[model], usage);
    scope->entries.push_back({unixSeconds, model, attribution, usage});
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

LocalUsageSnapshot LocalUsageReader::Scan(long long weeklyStartUnixSeconds) const {
    SYSTEMTIME now{}; GetLocalTime(&now);
    return ScanForLocalDate(now.wYear, now.wMonth, now.wDay, weeklyStartUnixSeconds);
}

LocalUsageSnapshot LocalUsageReader::ScanForLocalDate(int year, int month, int day, long long weeklyStartUnixSeconds) const {
    LocalUsageSnapshot result;
    // A remote weekly boundary makes an empty local history a valid, complete
    // zero-usage interval rather than an unavailable estimate.
    result.weekly.available = weeklyStartUnixSeconds > 0;
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
        AttributionSource attribution = AttributionSource::MissingOrExplicitUnpriced;
        bool malformed = false;
        while (std::getline(file, line)) {
            jsonlite::Parser parser(line); const auto parsed = parser.Parse();
            if (!parsed) { malformed = true; continue; }
            if (const auto canonicalModel = CanonicalModelFromMetadata(*parsed)) {
                model = canonicalModel->model;
                attribution = canonicalModel->source;
            }
            const auto* info = TokenUsageInfo(*parsed);
            const auto total = UsageFrom(info != nullptr ? info->Find("total_token_usage") : nullptr);
            if (!total) continue;
            Event event; event.date = ParseDate(*parsed, localUtcOffsetMinutesForTesting_); event.unixSeconds = ParseUnixSeconds(*parsed); event.model = model; event.attribution = attribution; event.total = *total;
            event.last = UsageFrom(info != nullptr ? info->Find("last_token_usage") : nullptr);
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
            if (event.last && &session == &sessions.back()) {
                result.last = {};
                AddToScope(&result.last, *event.last, event.model, event.attribution, event.unixSeconds);
            }
            if (!havePrevious) {
                AddToScope(&sessionTotal, event.total, event.model, event.attribution, event.unixSeconds);
                if (SameDate(event.date, today)) AddToScope(&result.today, event.total, event.model, event.attribution, event.unixSeconds);
                if (weeklyStartUnixSeconds > 0 && event.unixSeconds >= weeklyStartUnixSeconds) AddToScope(&result.weekly, event.total, event.model, event.attribution, event.unixSeconds);
                previous = event.total; havePrevious = true; continue;
            }
            if (!IsAtLeast(event.total, previous)) { previous = event.total; continue; }
            const TokenUsage delta = Difference(event.total, previous);
            AddToScope(&sessionTotal, delta, event.model, event.attribution, event.unixSeconds);
            if (SameDate(event.date, today)) AddToScope(&result.today, delta, event.model, event.attribution, event.unixSeconds);
            if (weeklyStartUnixSeconds > 0 && event.unixSeconds >= weeklyStartUnixSeconds) AddToScope(&result.weekly, delta, event.model, event.attribution, event.unixSeconds);
            previous = event.total;
        }
        result.tillNow.available = true;
        Add(&result.tillNow.usage, sessionTotal.usage);
        for (const auto& [model, usage] : sessionTotal.byModel) Add(&result.tillNow.byModel[model], usage);
        result.tillNow.entries.insert(result.tillNow.entries.end(), sessionTotal.entries.begin(), sessionTotal.entries.end());
        if (&session == &sessions.back()) result.task = sessionTotal;
    }
    return result;
}
