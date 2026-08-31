#pragma once

#include <map>
#include <string>
#include <vector>

struct TokenUsage {
    long long inputTokens = 0;
    long long cachedInputTokens = 0;
    long long cacheWriteInputTokens = 0;
    long long outputTokens = 0;
    long long totalTokens = 0;
};

// This is an in-memory accounting increment only. It never retains raw JSONL,
// prompts, authentication data, or any user content.
enum class AttributionSource {
    CanonicalMetadata,
    ValidatedFeatureMapping,
    MissingOrExplicitUnpriced,
};

enum class PrimaryModel {
    Auto,
    Gpt56Sol,
    Gpt56Terra,
    Gpt56Luna,
    Gpt55,
};

struct UsageLedgerEntry {
    long long unixSeconds = 0;
    std::wstring model;
    AttributionSource attribution = AttributionSource::MissingOrExplicitUnpriced;
    TokenUsage usage;
};

struct LocalUsageScope {
    bool available = false;
    TokenUsage usage;
    std::map<std::wstring, TokenUsage> byModel;
    std::vector<UsageLedgerEntry> entries;
};

struct CostEstimate {
    bool available = false;
    bool complete = false;
    double usd = 0.0;
    double confirmedUsd = 0.0;
    double estimatedUsd = 0.0;
    bool usedPrimaryModelFallback = false;
    bool hasUnpriced = false;
};

const wchar_t* PricingVersion();
const wchar_t* PrimaryModelSetting(PrimaryModel model);
PrimaryModel PrimaryModelFromSetting(const std::wstring& value);
const wchar_t* PrimaryModelDisplayName(PrimaryModel model);
CostEstimate EstimateApiEquivalentCost(const TokenUsage& usage, const std::wstring& model);
CostEstimate EstimateApiEquivalentCost(const LocalUsageScope& scope, PrimaryModel primaryModel = PrimaryModel::Auto);
