#pragma once

#include "Pricing.h"

#include <filesystem>

struct LocalUsageSnapshot {
    LocalUsageScope task;
    LocalUsageScope last;
    LocalUsageScope today;
    LocalUsageScope tillNow;
    size_t filesScanned = 0;
    size_t filesWithParseErrors = 0;
};

class LocalUsageReader {
public:
    explicit LocalUsageReader(std::filesystem::path codexHome = {});
    LocalUsageSnapshot Scan() const;
    LocalUsageSnapshot ScanForLocalDate(int year, int month, int day) const;

private:
    std::filesystem::path codexHome_;
};
