#include "analyzer.h"

#include <fstream>
#include <algorithm>
#include <queue>
#include <cctype>

using namespace std;

namespace {

static inline void trimInPlace(string& s) {
    size_t b = 0;
    while (b < s.size() && isspace((unsigned char)s[b])) ++b;
    size_t e = s.size();
    while (e > b && isspace((unsigned char)s[e - 1])) --e;
    if (b == 0 && e == s.size()) return;
    s = s.substr(b, e - b);
}

static inline vector<string> splitCSV6(const string& line) {
    vector<string> out;
    out.reserve(6);
    size_t start = 0;
    while (start <= line.size()) {
        size_t comma = line.find(',', start);
        if (comma == string::npos) comma = line.size();
        string field = line.substr(start, comma - start);
        trimInPlace(field);
        out.push_back(std::move(field));
        if (comma == line.size()) break;
        start = comma + 1;
    }
    return out;
}

static inline bool isLikelyHeader(const string& line) {
    return (line.find("TripID") != string::npos) &&
           (line.find("PickupZoneID") != string::npos);
}

static inline bool parseHour(const string& pickupDateTime, int& hourOut) {
    string s = pickupDateTime;
    trimInPlace(s);
    if (s.empty()) return false;

    size_t sep = s.find(' ');
    if (sep == string::npos) sep = s.find('T');
    if (sep == string::npos || sep + 1 >= s.size()) return false;

    string t = s.substr(sep + 1);
    trimInPlace(t);
    if (t.empty()) return false;

    size_t colon = t.find(':');
    if (colon == string::npos) return false;

    string h = t.substr(0, colon);
    trimInPlace(h);
    if (h.empty() || h.size() > 2) return false;

    int val = 0;
    for (char c : h) {
        if (!isdigit((unsigned char)c)) return false;
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
static vector<T> topKByHeap(const vector<T>& items, int k, Better better) {
    if (k <= 0 || items.empty()) return {};

    priority_queue<T, vector<T>, Better> pq(better);
    for (const auto& it : items) {
        if ((int)pq.size() < k) {
            pq.push(it);
        } else if (better(it, pq.top())) {
            pq.pop();
            pq.push(it);
        }
    }

    vector<T> out;
    out.reserve(pq.size());
    while (!pq.empty()) {
        out.push_back(pq.top());
        pq.pop();
    }
    sort(out.begin(), out.end(), better);
    return out;
}

} // namespace

void TripAnalyzer::ingestFile(const string& filename) {
    zoneToId_.clear();
    idToZone_.clear();
    zoneTotal_.clear();
    zoneHour_.clear();

    ifstream fin(filename);
    if (!fin.is_open()) return;

    string line;
    bool firstLine = true;
    zoneToId_.reserve(200000);

    while (getline(fin, line)) {
        if (firstLine) {
            firstLine = false;
            if (isLikelyHeader(line)) continue;
        }
        if (line.empty()) continue;

        auto fields = splitCSV6(line);
        if (fields.size() < 6) continue;

        const string& zone = fields[1];
        const string& dt   = fields[3];
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

vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    if (k <= 0) return {};

    vector<ZoneCount> all;
    all.reserve(idToZone_.size());
    for (size_t id = 0; id < idToZone_.size(); ++id) {
        all.push_back({ idToZone_[id], zoneTotal_[id] });
    }
    return topKByHeap(all, k, BetterZone{});
}

vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    if (k <= 0) return {};

    priority_queue<SlotCount, vector<SlotCount>, BetterSlot> pq(BetterSlot{});
    for (size_t id = 0; id < idToZone_.size(); ++id) {
        const auto& zone = idToZone_[id];
        const auto& hours = zoneHour_[id];
        for (int h = 0; h < 24; ++h) {
            long long cnt = hours[h];
            if (cnt <= 0) continue;

            SlotCount cand{ zone, h, cnt };
            if ((int)pq.size() < k) pq.push(cand);
            else if (BetterSlot{}(cand, pq.top())) {
                pq.pop();
                pq.push(cand);
            }
        }
    }

    vector<SlotCount> out;
    out.reserve(pq.size());
    while (!pq.empty()) {
        out.push_back(pq.top());
        pq.pop();
    }
    sort(out.begin(), out.end(), BetterSlot{});
    return out;
}
