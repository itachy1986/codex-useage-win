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
