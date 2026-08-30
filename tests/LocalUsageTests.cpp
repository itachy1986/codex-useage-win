#include "LocalUsageReader.h"
#include "Pricing.h"
#include "TaskbarPresentation.h"

#include <filesystem>
#include <fstream>
#include <cmath>
#include <chrono>
#include <iostream>
#include <string>

namespace {

int failures = 0;

void Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        ++failures;
    }
}

std::filesystem::path MakeFixtureRoot() {
    const auto root = std::filesystem::temp_directory_path() / "CodexUsageBarLocalUsageTests";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::create_directories(root / "sessions" / "2026" / "08", error);
    return root;
}

void WriteSession(const std::filesystem::path& root, const char* name, const std::string& lines) {
    const auto path = root / "sessions" / "2026" / "08" / name;
    std::ofstream out(path, std::ios::binary);
    out << lines;
    out.close();
    if (std::string(name) == "current.jsonl") {
        std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now() + std::chrono::seconds(10));
    }
}

std::string ThreadSettingsEvent(const char* timestamp, const char* model) {
    return "{\"timestamp\":\"" + std::string(timestamp)
        + "\",\"payload\":{\"type\":\"thread_settings_applied\",\"thread_settings\":{\"model\":\""
        + model + "\"}}}\n";
}

std::string TurnContextEvent(const char* timestamp, const char* model) {
    return "{\"timestamp\":\"" + std::string(timestamp)
        + "\",\"payload\":{\"type\":\"turn_context\",\"model\":\"" + model + "\"}}\n";
}

std::string TokenEvent(const char* timestamp, const char* model, int input, int cached, int cacheWrite,
    int output, int total, int lastTotal = -1) {
    std::string event = ThreadSettingsEvent(timestamp, model)
        + "{\"timestamp\":\"" + std::string(timestamp)
        + "\",\"payload\":{\"type\":\"token_count\",\"info\":{\"request\":{\"model\":\"unrelated-nested-model\"},\"total_token_usage\":{\"input_tokens\":" + std::to_string(input)
        + ",\"cached_input_tokens\":" + std::to_string(cached)
        + ",\"cache_write_input_tokens\":" + std::to_string(cacheWrite)
        + ",\"output_tokens\":" + std::to_string(output)
        + ",\"reasoning_output_tokens\":0,\"total_tokens\":" + std::to_string(total) + "}";
    if (lastTotal >= 0) {
        event += ",\"last_token_usage\":{\"input_tokens\":10,\"cached_input_tokens\":2,\"cache_write_input_tokens\":0,\"output_tokens\":5,\"reasoning_output_tokens\":0,\"total_tokens\":" + std::to_string(lastTotal) + "}";
    }
    return event + "}}}\n";
}

void TestAccounting() {
    const auto root = MakeFixtureRoot();
    WriteSession(root, "old.jsonl",
        TokenEvent("2026-08-29T23:59:00", "gpt-5.6-sol", 100, 20, 10, 20, 120)
        + TokenEvent("2026-08-30T00:01:00", "gpt-5.6-sol", 150, 30, 10, 30, 180));
    WriteSession(root, "current.jsonl",
        TokenEvent("2026-08-30T11:00:00", "gpt-5.6-sol", 1000, 200, 100, 300, 1300, 15)
        + TokenEvent("2026-08-30T11:02:00", "gpt-5.6-sol", 1000, 200, 100, 300, 1300, 15));

    LocalUsageReader reader(root, 0);
    const LocalUsageSnapshot snapshot = reader.ScanForLocalDate(2026, 8, 30);
    Expect(snapshot.task.available && snapshot.task.usage.totalTokens == 1300,
        "Task uses the newest cumulative snapshot without double counting");
    Expect(snapshot.last.available && snapshot.last.usage.totalTokens == 15,
        "Last uses Codex last_token_usage when available");
    Expect(snapshot.tillNow.usage.totalTokens == 1480,
        "Till Now aggregates latest snapshots across sessions once");
    Expect(snapshot.today.usage.totalTokens == 1360,
        "Today includes only local-date cumulative deltas");
    Expect(snapshot.today.usage.cachedInputTokens == 210,
        "Today preserves cached-input category arithmetic");
}

void TestSafeDegradationAndPricing() {
    const auto root = MakeFixtureRoot();
    WriteSession(root, "broken.jsonl", "{not-json}\n" + TokenEvent("2026-08-30T10:00:00", "unknown-model", 100, 0, 0, 10, 110));
    const LocalUsageSnapshot snapshot = LocalUsageReader(root, 0).ScanForLocalDate(2026, 8, 30);
    Expect(snapshot.filesWithParseErrors == 1, "Malformed JSONL degrades without aborting the scan");
    const CostEstimate unknown = EstimateApiEquivalentCost(snapshot.tillNow);
    Expect(!unknown.complete && unknown.available && unknown.usd == 0.0,
        "Unknown model is an explicit incomplete zero lower bound, never a guessed price");

    TokenUsage usage;
    usage.inputTokens = 1000000;
    usage.cachedInputTokens = 200000;
    usage.cacheWriteInputTokens = 100000;
    usage.outputTokens = 500000;
    usage.totalTokens = 1500000;
    const CostEstimate sol = EstimateApiEquivalentCost(usage, L"gpt-5.6-sol");
    Expect(sol.available && sol.complete && std::abs(sol.usd - 13.38) < 0.001,
        "Model pricing separates uncached, cached, cache-write, and output tokens");
    Expect(std::wstring(PricingVersion()).find(L"2026-08-30") != std::wstring::npos,
        "Pricing table carries an explicit effective date");
    const CostEstimate terra = EstimateApiEquivalentCost(usage, L"gpt-5.6-terra");
    Expect(terra.available && terra.complete && std::abs(terra.usd - 7.69) < 0.001,
        "GPT-5.6 Terra applies its verified cache-write multiplier");
    const CostEstimate luna = EstimateApiEquivalentCost(usage, L"gpt-5.6-luna");
    Expect(luna.available && luna.complete && std::abs(luna.usd - 0.769) < 0.001,
        "GPT-5.6 Luna applies its verified cache-write multiplier");
    const CostEstimate unknownCacheWrite = EstimateApiEquivalentCost(usage, L"unknown-model");
    Expect(!unknownCacheWrite.available && !unknownCacheWrite.complete,
        "Unknown model cache-write pricing remains incomplete rather than estimated");
}

void TestLocalDateAndMixedModelAttribution() {
    const auto dateRoot = MakeFixtureRoot();
    WriteSession(dateRoot, "current.jsonl", TokenEvent("2026-08-29T18:00:00-05:00", "gpt-5.6-sol", 100, 0, 0, 0, 100));
    const LocalUsageSnapshot localDate = LocalUsageReader(dateRoot, 480).ScanForLocalDate(2026, 8, 30);
    Expect(localDate.today.available && localDate.today.usage.totalTokens == 100,
        "Offset timestamps are assigned to the Windows-local calendar date");

    const auto modelRoot = MakeFixtureRoot();
    WriteSession(modelRoot, "current.jsonl",
        TokenEvent("2026-08-30T10:00:00Z", "gpt-5.6-sol", 100, 0, 0, 0, 100)
        + TokenEvent("2026-08-30T10:02:00Z", "gpt-5.6-terra", 300, 0, 0, 0, 300));
    const LocalUsageSnapshot mixed = LocalUsageReader(modelRoot, 0).ScanForLocalDate(2026, 8, 30);
    Expect(mixed.task.byModel.at(L"gpt-5.6-sol").totalTokens == 100
            && mixed.task.byModel.at(L"gpt-5.6-terra").totalTokens == 200,
        "A mixed-model session attributes cumulative deltas to the active model");
    const CostEstimate mixedCost = EstimateApiEquivalentCost(mixed.task);
    Expect(mixedCost.complete && std::abs(mixedCost.usd - 0.0008) < 0.000001,
        "A mixed-model session prices each model delta independently");

    const auto canonicalRoot = MakeFixtureRoot();
    WriteSession(canonicalRoot, "current.jsonl",
        TurnContextEvent("2026-08-30T10:00:00Z", "gpt-5.6-sol")
        + "{\"timestamp\":\"2026-08-30T10:00:00Z\",\"payload\":{\"type\":\"token_count\",\"info\":{\"tool\":{\"model\":\"unrelated-nested-model\"},\"total_token_usage\":{\"input_tokens\":100,\"cached_input_tokens\":0,\"cache_write_input_tokens\":0,\"output_tokens\":0,\"total_tokens\":100}}}}\n"
        + ThreadSettingsEvent("2026-08-30T10:01:00Z", "gpt-5.6-terra")
        + "{\"timestamp\":\"2026-08-30T10:02:00Z\",\"payload\":{\"type\":\"token_count\",\"info\":{\"tool\":{\"model\":\"still-unrelated\"},\"total_token_usage\":{\"input_tokens\":300,\"cached_input_tokens\":0,\"cache_write_input_tokens\":0,\"output_tokens\":0,\"total_tokens\":300}}}}\n");
    const LocalUsageSnapshot canonical = LocalUsageReader(canonicalRoot, 0).ScanForLocalDate(2026, 8, 30);
    Expect(canonical.task.byModel.count(L"gpt-5.6-sol") == 1
            && canonical.task.byModel.count(L"gpt-5.6-terra") == 1
            && canonical.task.byModel.count(L"unrelated-nested-model") == 0,
        "Only TurnContext/thread-settings canonical model metadata can attribute token deltas");
}

void TestWeeklyCycleAndIncompleteLifetimeCost() {
    const auto root = MakeFixtureRoot();
    WriteSession(root, "current.jsonl",
        TokenEvent("2026-08-30T09:00:00Z", "gpt-5.6-sol", 100, 0, 0, 0, 100)
        + TokenEvent("2026-08-30T10:00:00Z", "gpt-5.6-sol", 200, 0, 0, 0, 200));
    const LocalUsageSnapshot snapshot = LocalUsageReader(root, 0).ScanForLocalDate(2026, 8, 30, 1788084000);
    Expect(snapshot.weekly.available && snapshot.weekly.usage.totalTokens == 100,
        "Weekly cost window excludes cumulative usage before the remote cycle start");

    LocalUsageScope mixed;
    mixed.available = true;
    mixed.byModel[L"gpt-5.6-sol"].inputTokens = 1000000;
    mixed.byModel[L"unknown-model"].inputTokens = 1000000;
    const CostEstimate lowerBound = EstimateApiEquivalentCost(mixed);
    Expect(lowerBound.available && !lowerBound.complete && std::abs(lowerBound.usd - 4.0) < 0.000001,
        "Partially unpriced lifetime usage exposes a priced lower bound");

    LocalUsageScope zero;
    zero.available = true;
    zero.byModel[L"gpt-5.6-sol"] = {};
    const CostEstimate zeroCost = EstimateApiEquivalentCost(zero);
    Expect(zeroCost.available && zeroCost.complete && zeroCost.usd == 0.0,
        "A valid weekly cycle with zero local usage is a complete $0.00 estimate");

    const LocalUsageSnapshot emptyWeekly = LocalUsageReader(MakeFixtureRoot(), 0)
        .ScanForLocalDate(2026, 8, 30, 1788084000);
    const CostEstimate emptyWeeklyCost = EstimateApiEquivalentCost(emptyWeekly.weekly);
    Expect(emptyWeekly.weekly.available && emptyWeekly.weekly.usage.totalTokens == 0
            && emptyWeeklyCost.available && emptyWeeklyCost.complete && emptyWeeklyCost.usd == 0.0,
        "A remote weekly boundary with no local events remains a complete zero weekly estimate");

    LocalUsageScope allUnpriced;
    allUnpriced.available = true;
    allUnpriced.byModel[L"internal-unpriced-model"].inputTokens = 100;
    const CostEstimate allUnpricedCost = EstimateApiEquivalentCost(allUnpriced);
    Expect(allUnpricedCost.available && !allUnpricedCost.complete && allUnpricedCost.usd == 0.0,
        "All-unpriced weekly usage remains an explicit incomplete zero lower bound");
}

void TestTaskbarPresentation() {
    UsageSnapshot usage;
    usage.success = true;
    usage.fiveHour.available = true;
    usage.fiveHour.remainingPercent = 42;
    usage.weekly.available = true;
    usage.weekly.remainingPercent = 73;
    LocalUsageSnapshot local;
    local.weekly.available = true;
    local.weekly.byModel[L"gpt-5.6-sol"].inputTokens = 1000000;
    local.tillNow = local.weekly;

    const auto cards = BuildTaskbarMetricCards(usage, local);
    Expect(cards.size() == 4 && cards[0].label == L"5H" && cards[1].label == L"周"
            && cards[2].label == L"周消费" && cards[3].label == L"总消费",
        "Taskbar presentation exposes exactly the optional 5H plus weekly and two cost cards");
    bool hasSeparator = false;
    for (const auto& card : cards) hasSeparator = hasSeparator || card.label.find(L'|') != std::wstring::npos || card.value.find(L'|') != std::wstring::npos;
    Expect(!hasSeparator, "Taskbar card descriptors contain no legacy separators");

    usage.fiveHour.available = false;
    const auto withoutFiveHour = BuildTaskbarMetricCards(usage, local);
    Expect(withoutFiveHour.size() == 3 && withoutFiveHour[0].label == L"周",
        "Unavailable 5H removes the complete 5H taskbar card");
    const int withFiveWidth = CalculateTaskbarCardRowWidth({30, 36, 72, 72}, 4, 10, 4);
    const int withoutFiveWidth = CalculateTaskbarCardRowWidth({36, 72, 72}, 4, 10, 4);
    Expect(withFiveWidth > withoutFiveWidth && withoutFiveWidth != 500 && withoutFiveWidth != 360,
        "Taskbar width is content-driven and changes when visible cards change");
}

void TestStandardAndSimplePresentations() {
    UsageSnapshot usage;
    usage.success = true;
    usage.fiveHour.available = true;
    usage.fiveHour.remainingPercent = 42;
    usage.weekly.available = true;
    usage.weekly.remainingPercent = 73;

    LocalUsageSnapshot local;
    local.task.available = true;
    local.task.usage.totalTokens = 1000000;
    local.task.byModel[L"gpt-5.6-sol"].inputTokens = 1000000;
    local.last.available = true;
    local.last.usage.totalTokens = 110;
    local.last.byModel[L"unpriced-model"].inputTokens = 100;
    local.today.available = true;
    local.weekly.available = true;
    local.weekly.byModel[L"gpt-5.6-sol"].inputTokens = 1000000;
    local.weekly.byModel[L"unpriced-model"].inputTokens = 100;
    local.tillNow.available = true;
    local.tillNow.usage.totalTokens = 100;
    local.tillNow.byModel[L"unpriced-model"].inputTokens = 100;

    const auto standard = BuildStandardUsageMetricCards(local);
    Expect(standard.size() == 5 && standard[0].label == L"Task" && standard[1].label == L"Last"
            && standard[2].label == L"Today" && standard[3].label == L"周消费" && standard[4].label == L"总消费",
        "Standard presentation exposes Task, Last, Today, weekly cost, and lifetime cost from shared scopes");
    Expect(standard[0].value.find(L"≈$4.00") != std::wstring::npos
            && standard[1].value.find(L"≥$0.00") != std::wstring::npos
            && standard[2].value.find(L"≈$0.00") != std::wstring::npos
            && standard[3].value.find(L"≥$4.00") != std::wstring::npos
            && standard[4].value.find(L"≥$0.00") != std::wstring::npos,
        "Standard preserves complete, lower-bound, and valid-zero pricing semantics");

    const auto simple = BuildSimpleMetricCards(usage, local);
    Expect(simple.size() == 4 && simple[0].label == L"5H" && simple[1].label == L"周"
            && simple[2].label == L"周消费" && simple[3].label == L"总消费",
        "Simple presentation exposes only optional 5H, weekly quota, weekly cost, and lifetime cost");
    usage.fiveHour.available = false;
    const auto simpleWithoutFiveHour = BuildSimpleMetricCards(usage, local);
    Expect(simpleWithoutFiveHour.size() == 3 && simpleWithoutFiveHour[0].label == L"周",
        "Simple removes its complete 5H card when the remote lane is unavailable");
    for (const auto& card : standard) {
        Expect(card.label.find(L'|') == std::wstring::npos && card.value.find(L'|') == std::wstring::npos,
            "Standard card descriptors never regress to flat pipe-separated status text");
    }
}

void TestFixturesAreRedacted() {
    const std::string fixture = TokenEvent("2026-08-30T10:00:00", "gpt-5.6-sol", 1, 0, 0, 1, 2);
    Expect(fixture.find("access_token") == std::string::npos && fixture.find("content") == std::string::npos,
        "Synthetic fixtures contain no credentials or conversation content");
}

}  // namespace

int main() {
    TestAccounting();
    TestSafeDegradationAndPricing();
    TestLocalDateAndMixedModelAttribution();
    TestWeeklyCycleAndIncompleteLifetimeCost();
    TestTaskbarPresentation();
    TestStandardAndSimplePresentations();
    TestFixturesAreRedacted();
    return failures == 0 ? 0 : 1;
}
