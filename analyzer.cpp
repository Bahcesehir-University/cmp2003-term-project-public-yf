// analyzer.cpp
#include "analyzer.h"

#include <iostream>
#include <vector>
#include <string>
#include <unordered_map>
#include <array>
#include <algorithm>
#include <fstream>
#include <queue>
#include <cctype>

using namespace std;

namespace {

static inline void trimInPlace(std::string& s) {
    size_t b = 0;
    while (b < s.size() && std::isspace((unsigned char)s[b])) ++b;
    size_t e = s.size();
    while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
    if (b == 0 && e == s.size()) return;
    s = s.substr(b, e - b);
}

static inline std::vector<std::string> splitCSV6(const std::string& line) {
    std::vector<std::string> out;
    out.reserve(6);
    size_t start = 0;
    while (start <= line.size()) {
        size_t comma = line.find(',', start);
        if (comma == std::string::npos) comma = line.size();
        std::string field = line.substr(start, comma - start);
        trimInPlace(field);
        out.push_back(std::move(field));
        if (comma == line.size()) break;
        start = comma + 1;
    }
    return out;
}

static inline bool isLikelyHeader(const std::string& line) {
    return (line.find("TripID") != std::string::npos) &&
           (line.find("PickupZoneID") != std::string::npos);
}

static inline bool parseHour(const std::string& pickupDateTime, int& hourOut) {
    std::string s = pickupDateTime;
    trimInPlace(s);
    if (s.empty()) return false;

    size_t sep = s.find(' ');
    if (sep == std::string::npos) sep = s.find('T');
    if (sep == std::string::npos || sep + 1 >= s.size()) return false;

    std::string t = s.substr(sep + 1);
    trimInPlace(t);
    if (t.empty()) return false;

    size_t colon = t.find(':');
    if (colon == std::string::npos) return false;

    std::string h = t.substr(0, colon);
    trimInPlace(h);
    if (h.empty() || h.size() > 2) return false;

    int val = 0;
    for (char c : h) {
        if (!std::isdigit((unsigned char)c)) return false;
        val = val * 10 + (c - '0');
    }
    if (val < 0 || val > 23) return false;

    hourOut = val;
    return true;
}

struct BetterZone {
    bool operator()(const ZoneCount& a, const ZoneCount& b) const {
        if (a.count != b.count) return a.count > b.count;
        return a.zone < b.zone;
    }
};

struct BetterSlot {
    bool operator()(const SlotCount& a, const SlotCount& b) const {
        if (a.count != b.count) return a.count > b.count;
        if (a.zone != b.zone) return a.zone < b.zone;
        return a.hour < b.hour;
    }
};

template <class T, class Better>
static std::vector<T> topKByHeap(const std::vector<T>& items, int k, Better better) {
    if (k <= 0) return {};
    if (items.empty()) return {};

    std::priority_queue<T, std::vector<T>, Better> pq(better);

    for (const auto& it : items) {
        if ((int)pq.size() < k) {
            pq.push(it);
        } else if (better(it, pq.top())) {
            pq.pop();
            pq.push(it);
        }
    }

    std::vector<T> out;
    out.reserve(pq.size());
    while (!pq.empty()) {
        out.push_back(pq.top());
        pq.pop();
    }

    std::sort(out.begin(), out.end(), better);
    return out;
}

} // namespace

void TripAnalyzer::ingestStdin() {
    zoneToId_.clear();
    idToZone_.clear();
    zoneTotal_.clear();
    zoneHour_.clear();

    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    std::string line;
    bool firstLine = true;
    zoneToId_.reserve(200000);

    while (std::getline(std::cin, line)) {
        if (firstLine) {
            firstLine = false;
            if (isLikelyHeader(line)) continue;
        }
        if (line.empty()) continue;

        auto fields = splitCSV6(line);
        if (fields.size() < 6) continue;

        const std::string& zone = fields[1];
        const std::string& dt   = fields[3];
        if (zone.empty() || dt.empty()) continue;

        int hour = -1;
        if (!parseHour(dt, hour)) continue;

        int id;
        auto it = zoneToId_.find(zone);
        if (it == zoneToId_.end()) {
            id = (int)idToZone_.size();
            zoneToId_.emplace(zone, id);
            idToZone_.push_back(zone);
            zoneTotal_.push_back(0);
            zoneHour_.push_back({});
        } else {
            id = it->second;
        }

        ++zoneTotal_[id];
        ++zoneHour_[id][hour];
    }
}

std::vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    if (k <= 0) return {};

    std::vector<ZoneCount> all;
    all.reserve(idToZone_.size());
    for (size_t id = 0; id < idToZone_.size(); ++id) {
        all.push_back(ZoneCount{ idToZone_[id], zoneTotal_[id] });
    }

    return topKByHeap(all, k, BetterZone{});
}

std::vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    if (k <= 0) return {};

    std::priority_queue<SlotCount, std::vector<SlotCount>, BetterSlot> pq(BetterSlot{});

    for (size_t id = 0; id < idToZone_.size(); ++id) {
        const std::string& zone = idToZone_[id];
        const auto& hours = zoneHour_[id];
        for (int h = 0; h < 24; ++h) {
            long long cnt = hours[h];
            if (cnt <= 0) continue;

            SlotCount cand{ zone, h, cnt };
            if ((int)pq.size() < k) {
                pq.push(cand);
            } else if (BetterSlot{}(cand, pq.top())) {
                pq.pop();
                pq.push(cand);
            }
        }
    }

    std::vector<SlotCount> out;
    out.reserve(pq.size());
    while (!pq.empty()) {
        out.push_back(pq.top());
        pq.pop();
    }
    std::sort(out.begin(), out.end(), BetterSlot{});
    return out;
}
