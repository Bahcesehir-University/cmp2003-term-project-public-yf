#include "analyzer.h"
#include <unordered_map>
#include <vector>
#include <array>
#include <string>
#include <fstream>
#include <algorithm>
#include <cctype>

using namespace std;

namespace {

inline void trim(string& s) {
    size_t b = 0;
    while (b < s.size() && isspace((unsigned char)s[b])) b++;
    size_t e = s.size();
    while (e > b && isspace((unsigned char)s[e - 1])) e--;
    s = s.substr(b, e - b);
}

vector<string> splitCSV(const string& line) {
    vector<string> out;
    size_t start = 0;
    while (start <= line.size()) {
        size_t pos = line.find(',', start);
        if (pos == string::npos) pos = line.size();
        string field = line.substr(start, pos - start);
        trim(field);
        out.push_back(field);
        if (pos == line.size()) break;
        start = pos + 1;
    }
    return out;
}

bool parseHour(const string& dt, int& hour) {
    size_t space = dt.find(' ');
    if (space == string::npos || space + 1 >= dt.size()) return false;

    size_t colon = dt.find(':', space + 1);
    if (colon == string::npos) return false;

    string h = dt.substr(space + 1, colon - (space + 1));
    if (h.empty() || h.size() > 2) return false;

    int val = 0;
    for (char c : h) {
        if (!isdigit((unsigned char)c)) return false;
        val = val * 10 + (c - '0');
    }

    if (val < 0 || val > 23) return false;
    hour = val;
    return true;
}

}

static unordered_map<string, int> zoneId;
static vector<string> idZone;
static vector<long long> zoneTotal;
static vector<array<long long, 24>> zoneHour;

void TripAnalyzer::ingestFile(const string& csvPath) {
    zoneId.clear();
    idZone.clear();
    zoneTotal.clear();
    zoneHour.clear();

    ifstream file(csvPath);
    if (!file.is_open()) return;

    string line;
    bool first = true;

    while (getline(file, line)) {
        if (first) { first = false; continue; }
        if (line.empty()) continue;

        auto fields = splitCSV(line);
        if (fields.size() < 3) continue;

        const string& zone = fields[1];
        const string& dt   = fields[2];
        if (zone.empty() || dt.empty()) continue;

        int hour;
        if (!parseHour(dt, hour)) continue;

        int id;
        auto it = zoneId.find(zone);
        if (it == zoneId.end()) {
            id = (int)idZone.size();
            zoneId[zone] = id;
            idZone.push_back(zone);
            zoneTotal.push_back(0);
            zoneHour.push_back({});
        } else {
            id = it->second;
        }

        zoneTotal[id]++;
        zoneHour[id][hour]++;
    }
}

vector<ZoneCount> TripAnalyzer::topZones(int k) const {
    vector<ZoneCount> all;
    for (size_t i = 0; i < idZone.size(); i++)
        all.push_back({idZone[i], zoneTotal[i]});

    sort(all.begin(), all.end(), [](const ZoneCount& a, const ZoneCount& b) {
        if (a.count != b.count) return a.count > b.count;
        return a.zone < b.zone;
    });

    if ((int)all.size() > k) all.resize(k);
    return all;
}

vector<SlotCount> TripAnalyzer::topBusySlots(int k) const {
    vector<SlotCount> all;

    for (size_t i = 0; i < idZone.size(); i++) {
        for (int h = 0; h < 24; h++) {
            if (zoneHour[i][h] > 0)
                all.push_back({idZone[i], h, zoneHour[i][h]});
        }
    }

    sort(all.begin(), all.end(), [](const SlotCount& a, const SlotCount& b) {
        if (a.count != b.count) return a.count > b.count;
        if (a.zone != b.zone) return a.zone < b.zone;
        return a.hour < b.hour;
    });

    if ((int)all.size() > k) all.resize(k);
    return all;
}
