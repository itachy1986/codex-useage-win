#include "LocalUsageReader.h"
#include "Pricing.h"

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

std::string TokenEvent(const char* timestamp, const char* model, int input, int cached, int cacheWrite,
    int output, int total, int lastTotal = -1) {
    std::string event = "{\"timestamp\":\"" + std::string(timestamp) + "\",\"payload\":{\"type\":\"token_count\",\"model\":\"" + model
        + "\",\"info\":{\"total_token_usage\":{\"input_tokens\":" + std::to_string(input)
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
    Expect(!unknown.complete && !unknown.available, "Unknown model is incomplete and never priced as zero");

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
    const CostEstimate unverifiedCacheWrite = EstimateApiEquivalentCost(usage, L"gpt-5.6-terra");
    Expect(!unverifiedCacheWrite.available && !unverifiedCacheWrite.complete,
        "Unverified model cache-write pricing is incomplete rather than estimated");
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
    TestFixturesAreRedacted();
    return failures == 0 ? 0 : 1;
}
