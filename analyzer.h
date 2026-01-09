#ifndef ANALYZER_H
#define ANALYZER_H

#include <vector>
#include <string>
#include <unordered_map>
#include <array>

struct ZoneCount {
    std::string zone;
    long long count;
};

struct SlotCount {
    std::string zone;
    int hour;
    long long count;
};

class TripAnalyzer {
public:
    void ingestFile(const std::string& csvPath);
    std::vector<ZoneCount> topZones(int k = 10) const;
    std::vector<SlotCount> topBusySlots(int k = 10) const;

private:
    std::unordered_map<std::string, int> zoneToId_;
    std::vector<std::string> idToZone_;
    std::vector<long long> zoneTotal_;
    std::vector<std::array<long long, 24>> zoneHour_;
};

#endif
