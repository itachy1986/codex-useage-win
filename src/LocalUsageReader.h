#pragma once

#include "Pricing.h"

#include <filesystem>
#include <optional>

struct LocalUsageSnapshot {
    LocalUsageScope task;
    LocalUsageScope last;
    LocalUsageScope today;
    LocalUsageScope weekly;
    LocalUsageScope tillNow;
    size_t filesScanned = 0;
    size_t filesWithParseErrors = 0;
};

class LocalUsageReader {
public:
    explicit LocalUsageReader(std::filesystem::path codexHome = {}, std::optional<int> localUtcOffsetMinutesForTesting = std::nullopt);
    LocalUsageSnapshot Scan(long long weeklyStartUnixSeconds = 0) const;
    LocalUsageSnapshot ScanForLocalDate(int year, int month, int day, long long weeklyStartUnixSeconds = 0) const;

private:
    std::filesystem::path codexHome_;
    std::optional<int> localUtcOffsetMinutesForTesting_;
};
