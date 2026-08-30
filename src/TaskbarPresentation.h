#pragma once

#include "CodexUsageFetcher.h"
#include "LocalUsageReader.h"

#include <string>
#include <vector>

struct TaskbarMetricCard {
    std::wstring label;
    std::wstring value;
};

// The taskbar intentionally exposes only its four compact status metrics.
std::vector<TaskbarMetricCard> BuildTaskbarMetricCards(
    const UsageSnapshot& usage,
    const LocalUsageSnapshot& localUsage);

// Width is derived from the measured content of each card; callers provide
// current-DPI text widths so no fixed taskbar canvas is baked into layout.
int CalculateTaskbarCardRowWidth(
    const std::vector<int>& cardContentWidths,
    int outerPadding,
    int cardHorizontalPadding,
    int gap);
