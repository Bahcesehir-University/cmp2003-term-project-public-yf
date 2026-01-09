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

static void ingestFromStream(std::istream& in,
                            std::unordered_map<std::string, int>& zoneToId,
                            std::vector<std::string>& idToZone,
                            std::vector<long long>& zoneTotal,
                            std::vector<std::array<long long, 24>>& zoneHour) {
    zoneToId.clear();
    idToZone.clear();
    zoneTotal.clear();
    zoneHour.clear();

    std::string line;
    bool firstLine = true;
    zoneToId.reserve(200000);

    while (std::getline(in, line)) {
        if (firstLine
