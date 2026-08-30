#pragma once

#include <map>
#include <string>

struct TokenUsage {
    long long inputTokens = 0;
    long long cachedInputTokens = 0;
    long long cacheWriteInputTokens = 0;
    long long outputTokens = 0;
    long long totalTokens = 0;
};

struct LocalUsageScope {
    bool available = false;
    TokenUsage usage;
    std::map<std::wstring, TokenUsage> byModel;
};

struct CostEstimate {
    bool available = false;
    bool complete = false;
    double usd = 0.0;
};

const wchar_t* PricingVersion();
CostEstimate EstimateApiEquivalentCost(const TokenUsage& usage, const std::wstring& model);
CostEstimate EstimateApiEquivalentCost(const LocalUsageScope& scope);
