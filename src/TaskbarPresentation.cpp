#include "TaskbarPresentation.h"

#include "Pricing.h"

#include <algorithm>
#include <cwchar>

namespace {

std::wstring Money(double value) {
    wchar_t money[32] = {};
    swprintf_s(money, L"$%.2f", value);
    return money;
}

std::wstring FormatCost(const LocalUsageScope& scope, PrimaryModel primaryModel, bool detailed = false) {
    const CostEstimate cost = EstimateApiEquivalentCost(scope, primaryModel);
    if (!cost.available) return L"N/A";
    if (cost.hasUnpriced) {
        std::wstring value = L"≥" + Money(cost.confirmedUsd);
        if (detailed && cost.estimatedUsd > 0.0) value += L" + " + Money(cost.estimatedUsd) + L" estimated";
        return value;
    }
    if (cost.usedPrimaryModelFallback) {
        return L"~" + Money(cost.usd) + (detailed ? L" (estimated)" : L"");
    }
    return L"≈" + Money(cost.confirmedUsd);
}

std::wstring FormatCompactTokens(long long tokens) {
    const wchar_t* unit = L"";
    double value = static_cast<double>(tokens);
    if (tokens >= 1000000000LL) { value /= 1000000000.0; unit = L"B"; }
    else if (tokens >= 1000000LL) { value /= 1000000.0; unit = L"M"; }
    else if (tokens >= 1000LL) { value /= 1000.0; unit = L"K"; }
    wchar_t buffer[32] = {};
    swprintf_s(buffer, L"%.1f%s", value, unit);
    return buffer;
}

std::wstring FormatTokenAndCost(const LocalUsageScope& scope, PrimaryModel primaryModel, bool detailed = false) {
    if (!scope.available) return L"N/A";
    return FormatCompactTokens(scope.usage.totalTokens) + L" · " + FormatCost(scope, primaryModel, detailed);
}

std::wstring FormatRemaining(bool success, const UsageWindow& window) {
    return success && window.available ? std::to_wstring(window.remainingPercent) + L"%" : L"N/A";
}

}  // namespace

std::vector<TaskbarMetricCard> BuildTaskbarMetricCards(
    const UsageSnapshot& usage,
    const LocalUsageSnapshot& localUsage,
    PrimaryModel primaryModel) {
    std::vector<TaskbarMetricCard> cards;
    if (usage.success && usage.fiveHour.available) {
        cards.push_back({L"5H", std::to_wstring(usage.fiveHour.remainingPercent) + L"%"});
    }
    cards.push_back({L"周", FormatRemaining(usage.success, usage.weekly)});
    cards.push_back({L"周消费", FormatCost(localUsage.weekly, primaryModel)});
    cards.push_back({L"总消费", FormatCost(localUsage.tillNow, primaryModel)});
    return cards;
}

std::vector<TaskbarMetricCard> BuildSimpleMetricCards(
    const UsageSnapshot& usage,
    const LocalUsageSnapshot& localUsage,
    PrimaryModel primaryModel) {
    return BuildTaskbarMetricCards(usage, localUsage, primaryModel);
}

std::vector<TaskbarMetricCard> BuildStandardUsageMetricCards(
    const LocalUsageSnapshot& localUsage,
    PrimaryModel primaryModel) {
    return {
        {L"Task", FormatTokenAndCost(localUsage.task, primaryModel, true)},
        {L"Last", FormatTokenAndCost(localUsage.last, primaryModel, true)},
        {L"Today", FormatTokenAndCost(localUsage.today, primaryModel, true)},
        {L"周消费", FormatCost(localUsage.weekly, primaryModel, true)},
        {L"总消费", FormatTokenAndCost(localUsage.tillNow, primaryModel, true)},
    };
}

int CalculateTaskbarCardRowWidth(
    const std::vector<int>& cardContentWidths,
    int outerPadding,
    int cardHorizontalPadding,
    int gap) {
    int width = std::max(0, outerPadding) * 2;
    for (const int contentWidth : cardContentWidths) {
        width += std::max(0, contentWidth) + std::max(0, cardHorizontalPadding) * 2;
    }
    if (cardContentWidths.size() > 1) {
        width += static_cast<int>(cardContentWidths.size() - 1) * std::max(0, gap);
    }
    return width;
}
