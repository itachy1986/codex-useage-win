#include "Pricing.h"

#include <algorithm>
#include <cwctype>
#include <optional>

namespace {

struct Rate {
    double inputPerMillion;
    double cachedPerMillion;
    double outputPerMillion;
    std::optional<double> cacheWriteMultiplier;
    bool longContextSupported;
};

std::wstring Normalize(std::wstring model) {
    std::transform(model.begin(), model.end(), model.begin(), towlower);
    return model;
}

const Rate* FindRate(const std::wstring& rawModel) {
    // OpenAI ChatGPT Work / Codex Rate Card and model pages, verified 2026-08-31.
    // Cache-write billing is documented only for GPT-5.6 and later.
    static const Rate sol{4.0, 0.4, 20.0, 1.25, true};
    static const Rate terra{2.0, 0.2, 12.0, 1.25, true};
    static const Rate luna{0.2, 0.02, 1.2, 1.25, true};
    static const Rate gpt55{5.0, 0.5, 30.0, std::nullopt, true};
    static const Rate gpt54{2.5, 0.25, 15.0, std::nullopt, true};
    static const Rate gpt54Mini{0.75, 0.075, 4.5, std::nullopt, false};
    static const Rate gpt53Codex{1.75, 0.175, 14.0, std::nullopt, false};
    static const Rate gpt52{1.75, 0.175, 14.0, std::nullopt, false};
    const std::wstring model = Normalize(rawModel);
    if (model == L"gpt-5.6" || model == L"gpt-5.6-sol") return &sol;
    if (model == L"gpt-5.6-terra") return &terra;
    if (model == L"gpt-5.6-luna") return &luna;
    if (model == L"gpt-5.5") return &gpt55;
    if (model == L"gpt-5.4") return &gpt54;
    if (model == L"gpt-5.4-mini") return &gpt54Mini;
    if (model == L"gpt-5.3-codex") return &gpt53Codex;
    if (model == L"gpt-5.2") return &gpt52;
    return nullptr;
}

const wchar_t* CanonicalPrimaryModel(PrimaryModel model) {
    switch (model) {
        case PrimaryModel::Gpt56Sol: return L"gpt-5.6-sol";
        case PrimaryModel::Gpt56Terra: return L"gpt-5.6-terra";
        case PrimaryModel::Gpt56Luna: return L"gpt-5.6-luna";
        case PrimaryModel::Gpt55: return L"gpt-5.5";
        case PrimaryModel::Auto: return L"";
    }
    return L"";
}

CostEstimate PriceKnownUsage(const TokenUsage& usage, const std::wstring& model, bool applyLongContext = true) {
    const Rate* rate = FindRate(model);
    if (rate == nullptr) return {};
    const long long cached = std::max(0LL, usage.cachedInputTokens);
    const long long cacheWrite = std::max(0LL, usage.cacheWriteInputTokens);
    const long long uncached = std::max(0LL, usage.inputTokens - cached - cacheWrite);
    const bool longContext = applyLongContext && rate->longContextSupported && std::max(0LL, usage.inputTokens) > 272000;
    const double inputMultiplier = longContext ? 2.0 : 1.0;
    const double outputMultiplier = longContext ? 1.5 : 1.0;
    CostEstimate result;
    result.available = true;
    result.complete = cacheWrite == 0 || rate->cacheWriteMultiplier.has_value();
    result.hasUnpriced = !result.complete;
    result.usd = (uncached * rate->inputPerMillion * inputMultiplier
        + cached * rate->cachedPerMillion * inputMultiplier
        + std::max(0LL, usage.outputTokens) * rate->outputPerMillion * outputMultiplier) / 1000000.0;
    if (cacheWrite > 0 && rate->cacheWriteMultiplier.has_value()) {
        result.usd += cacheWrite * rate->inputPerMillion * *rate->cacheWriteMultiplier * inputMultiplier / 1000000.0;
    }
    result.confirmedUsd = result.usd;
    return result;
}

void AddPart(CostEstimate* total, const CostEstimate& part, bool estimated) {
    total->available = true;
    if (!part.available) {
        total->complete = false;
        total->hasUnpriced = true;
        return;
    }
    total->usd += part.usd;
    if (estimated) {
        total->estimatedUsd += part.usd;
        total->usedPrimaryModelFallback = true;
    } else {
        total->confirmedUsd += part.usd;
    }
    if (!part.complete || part.hasUnpriced) {
        total->complete = false;
        total->hasUnpriced = true;
    }
}

CostEstimate PriceEntry(const UsageLedgerEntry& entry, PrimaryModel primaryModel) {
    if (!entry.model.empty()) return PriceKnownUsage(entry.usage, entry.model);
    if (primaryModel == PrimaryModel::Auto) return {};
    CostEstimate result = PriceKnownUsage(entry.usage, CanonicalPrimaryModel(primaryModel));
    result.confirmedUsd = 0.0;
    result.estimatedUsd = result.usd;
    result.usedPrimaryModelFallback = result.available;
    return result;
}

}  // namespace

const wchar_t* PricingVersion() {
    return L"OpenAI ChatGPT Work/Codex Rate Card + model pages, verified 2026-08-31";
}

const wchar_t* PrimaryModelSetting(PrimaryModel model) {
    switch (model) {
        case PrimaryModel::Gpt56Sol: return L"sol";
        case PrimaryModel::Gpt56Terra: return L"terra";
        case PrimaryModel::Gpt56Luna: return L"luna";
        case PrimaryModel::Gpt55: return L"gpt-5.5";
        case PrimaryModel::Auto: return L"auto";
    }
    return L"auto";
}

PrimaryModel PrimaryModelFromSetting(const std::wstring& value) {
    const std::wstring normalized = Normalize(value);
    if (normalized == L"sol") return PrimaryModel::Gpt56Sol;
    if (normalized == L"terra") return PrimaryModel::Gpt56Terra;
    if (normalized == L"luna") return PrimaryModel::Gpt56Luna;
    if (normalized == L"gpt-5.5") return PrimaryModel::Gpt55;
    return PrimaryModel::Auto;
}

const wchar_t* PrimaryModelDisplayName(PrimaryModel model) {
    switch (model) {
        case PrimaryModel::Gpt56Sol: return L"GPT-5.6 Sol";
        case PrimaryModel::Gpt56Terra: return L"GPT-5.6 Terra";
        case PrimaryModel::Gpt56Luna: return L"GPT-5.6 Luna";
        case PrimaryModel::Gpt55: return L"GPT-5.5";
        case PrimaryModel::Auto: return L"Auto";
    }
    return L"Auto";
}

CostEstimate EstimateApiEquivalentCost(const TokenUsage& usage, const std::wstring& model) {
    return PriceKnownUsage(usage, model);
}

CostEstimate EstimateApiEquivalentCost(const LocalUsageScope& scope, PrimaryModel primaryModel) {
    if (!scope.available) return {};
    CostEstimate result;
    result.available = true;
    result.complete = true;
    if (!scope.entries.empty()) {
        for (const UsageLedgerEntry& entry : scope.entries) {
            const CostEstimate part = PriceEntry(entry, primaryModel);
            const bool estimated = entry.model.empty() && primaryModel != PrimaryModel::Auto && part.available;
            AddPart(&result, part, estimated);
        }
    } else {
        // Compatibility path for grouped data. Reader-produced scopes use entries.
        for (const auto& [model, usage] : scope.byModel) {
            UsageLedgerEntry entry;
            entry.model = model;
            entry.usage = usage;
            // Aggregated compatibility data has no per-request boundary, so it
            // must not accidentally trigger a long-context multiplier.
            CostEstimate part;
            if (model.empty() && primaryModel != PrimaryModel::Auto) {
                part = PriceKnownUsage(usage, CanonicalPrimaryModel(primaryModel), false);
                part.confirmedUsd = 0.0;
                part.estimatedUsd = part.usd;
                part.usedPrimaryModelFallback = part.available;
            } else {
                part = PriceKnownUsage(usage, model, false);
            }
            const bool estimated = model.empty() && primaryModel != PrimaryModel::Auto && part.available;
            AddPart(&result, part, estimated);
        }
    }
    result.usd = result.confirmedUsd + result.estimatedUsd;
    return result;
}
