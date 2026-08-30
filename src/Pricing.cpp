#include "Pricing.h"

#include <algorithm>
#include <cwctype>

namespace {

struct Rate {
    double inputPerMillion;
    double cachedPerMillion;
    double outputPerMillion;
};

std::wstring Normalize(std::wstring model) {
    std::transform(model.begin(), model.end(), model.begin(), towlower);
    return model;
}

const Rate* FindRate(const std::wstring& rawModel) {
    static const Rate sol{4.0, 0.4, 20.0};
    static const Rate gpt55{5.0, 0.5, 30.0};
    static const Rate terra{2.0, 0.2, 12.0};
    static const Rate luna{0.2, 0.02, 1.2};
    const std::wstring model = Normalize(rawModel);
    if (model == L"gpt-5.6" || model == L"gpt-5.6-sol") return &sol;
    if (model == L"gpt-5.5") return &gpt55;
    if (model == L"gpt-5.6-terra") return &terra;
    if (model == L"gpt-5.6-luna") return &luna;
    return nullptr;
}

}  // namespace

const wchar_t* PricingVersion() {
    return L"OpenAI API model pages, effective 2026-08-30";
}

CostEstimate EstimateApiEquivalentCost(const TokenUsage& usage, const std::wstring& model) {
    const Rate* rate = FindRate(model);
    if (rate == nullptr) return {};
    const long long cached = std::max(0LL, usage.cachedInputTokens);
    const long long cacheWrite = std::max(0LL, usage.cacheWriteInputTokens);
    const long long uncached = std::max(0LL, usage.inputTokens - cached - cacheWrite);
    CostEstimate result;
    result.available = true;
    result.complete = true;
    result.usd = (uncached * rate->inputPerMillion + cached * rate->cachedPerMillion
        + cacheWrite * rate->inputPerMillion * 1.25 + std::max(0LL, usage.outputTokens) * rate->outputPerMillion) / 1000000.0;
    return result;
}

CostEstimate EstimateApiEquivalentCost(const LocalUsageScope& scope) {
    if (!scope.available || scope.byModel.empty()) return {};
    CostEstimate result;
    result.available = true;
    result.complete = true;
    for (const auto& [model, usage] : scope.byModel) {
        const CostEstimate part = EstimateApiEquivalentCost(usage, model);
        if (!part.available) {
            result.available = false;
            result.complete = false;
            return result;
        }
        result.usd += part.usd;
    }
    return result;
}
