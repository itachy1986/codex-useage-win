#include "TaskbarPresentation.h"

#include "Pricing.h"

#include <algorithm>
#include <cwchar>

namespace {

std::wstring FormatCost(const LocalUsageScope& scope) {
    const CostEstimate cost = EstimateApiEquivalentCost(scope);
    if (!cost.available) return L"N/A";
    wchar_t money[32] = {};
    swprintf_s(money, cost.complete ? L"≈$%.2f" : L"≥$%.2f", cost.usd);
    return money;
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

std::wstring FormatTokenAndCost(const LocalUsageScope& scope) {
    if (!scope.available) return L"N/A";
    return FormatCompactTokens(scope.usage.totalTokens) + L" · " + FormatCost(scope);
}

std::wstring FormatRemaining(bool success, const UsageWindow& window) {
    return success && window.available ? std::to_wstring(window.remainingPercent) + L"%" : L"N/A";
}

}  // namespace

std::vector<TaskbarMetricCard> BuildTaskbarMetricCards(
    const UsageSnapshot& usage,
    const LocalUsageSnapshot& localUsage) {
    std::vector<TaskbarMetricCard> cards;
    if (usage.success && usage.fiveHour.available) {
        cards.push_back({L"5H", std::to_wstring(usage.fiveHour.remainingPercent) + L"%"});
    }
    cards.push_back({L"周", FormatRemaining(usage.success, usage.weekly)});
    cards.push_back({L"周消费", FormatCost(localUsage.weekly)});
    cards.push_back({L"总消费", FormatCost(localUsage.tillNow)});
    return cards;
}

std::vector<TaskbarMetricCard> BuildSimpleMetricCards(
    const UsageSnapshot& usage,
    const LocalUsageSnapshot& localUsage) {
    return BuildTaskbarMetricCards(usage, localUsage);
}

std::vector<TaskbarMetricCard> BuildStandardUsageMetricCards(
    const LocalUsageSnapshot& localUsage) {
    return {
        {L"Task", FormatTokenAndCost(localUsage.task)},
        {L"Last", FormatTokenAndCost(localUsage.last)},
        {L"Today", FormatTokenAndCost(localUsage.today)},
        {L"周消费", FormatCost(localUsage.weekly)},
        {L"总消费", FormatTokenAndCost(localUsage.tillNow)},
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
