#include "analyzer.h"

#include <fstream>
#include <algorithm>
#include <queue>
#include <cctype>

using namespace std;

namespace {

static inline void trimRange(const std::string& s, size_t& b, size_t& e) {
    while (b < e && std::isspace((unsigned char)s[b])) ++b;
    while (e > b && std::isspace((unsigned char)s[e - 1])) --e;
}

static inline bool parseHourFromPickupTimeField(const std::string& field, int& hourOut) {
    size_t b = 0, e = field.size();
    trimRange(field, b, e);
    if (b >= e) return false;

    size_t sep = field.find(' ', b);
    if (sep == std::string::npos || sep + 1 >= e) return false;

    size_t hb = sep + 1;
    while (hb < e && std::isspace((unsigned char)field[hb])) ++hb;
    if (hb >= e) return false;

    int val = 0;
    int digits = 0;
    size_t i = hb;

    while (i < e && std::isdigit((unsigned char)field[i]) && digits < 2) {
        val = val * 10 + (field[i] - '0');
        ++i;
        ++digits;
    }

    if (digits == 0) return false;
    if (i >= e || field[i] != ':') return false;
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
    if (k <= 0 || items.empty()) return {};

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

void TripAnalyzer::ingestFile(const std::string& csvPath) {
    zoneToId_.clear();
    idToZone_.clear();
    zoneTotal_.clear();
    zoneHour_.clear();

    std::ifstream fin(csvPath);
    if (!fin.is_open()) return;

    std::string line;

    if (!std::getline(fin, line)) return;

    zoneToId_.reserve(200000);

    while (std::getline(fin, line)) {
        if (line.empty()) continue;

        size_t c1 = line.find(',');
        if (c1 == std::string::npos) continue;

        size_t c2 = line.find(',', c1 + 1);
        if (c2 == std::string::npos) continue;

        if (line.find(',', c2 + 1) != std::string::npos) continue;

        size_t zb = c1 + 1, ze = c2;
        trimRange(line, zb, ze);
        if (zb >= ze) continue;

        std::string zone = line.substr(zb, ze - zb);

        size_t tb = c2 + 1, te = line.size();
        trimRange(line, tb, te);
        if (tb >= te) continue;

        int hour = -1;
        if (!parseHourFromPickupTimeField(line.substr(tb, te - tb), hour)) continue;

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

