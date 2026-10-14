#include "inference/units.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

namespace mutqin {

namespace {

size_t Utf8Length(unsigned char lead) {
    if (lead >= 0xF0)
        return 4;
    if (lead >= 0xE0)
        return 3;
    if (lead >= 0xC0)
        return 2;
    return 1;
}

}  // namespace

std::string CollapseRuns(const std::string& unit, int keep) {
    if (keep <= 0)
        return unit;
    std::string out, last;
    int run = 0;
    for (size_t i = 0; i < unit.size();) {
        const size_t n = std::min(Utf8Length(static_cast<unsigned char>(unit[i])), unit.size() - i);
        const std::string ch = unit.substr(i, n);
        run = ch == last ? run + 1 : 1;
        last = ch;
        if (run <= keep)
            out += ch;
        i += n;
    }
    return out;
}

bool LoadUnitGroups(const std::string& path, int surah, int ayah_first, int ayah_last,
                    std::vector<UnitGroup>* out, std::string* error) {
    std::ifstream in(path);
    if (!in) {
        *error = "cannot open " + path;
        return false;
    }
    out->clear();
    std::vector<bool> seen(static_cast<size_t>(ayah_last - ayah_first + 1), false);
    std::string line;
    int number = 0;
    while (std::getline(in, line)) {
        ++number;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty())
            continue;
        const size_t t1 = line.find('\t');
        const size_t t2 = t1 == std::string::npos ? t1 : line.find('\t', t1 + 1);
        const size_t colon = line.find(':');
        const size_t dash = t1 == std::string::npos ? std::string::npos : line.find('-', t1);
        if (t2 == std::string::npos || colon == std::string::npos || colon > t1 ||
            dash == std::string::npos || dash > t2) {
            *error =
                path + " line " + std::to_string(number) + ": expected surah:ayah<TAB>first-last<TAB>units";
            return false;
        }
        UnitGroup g;
        g.surah = std::atoi(line.substr(0, colon).c_str());
        if (g.surah != surah)
            continue;
        g.ayah = std::atoi(line.substr(colon + 1, t1 - colon - 1).c_str());
        if (g.ayah < ayah_first || g.ayah > ayah_last)
            continue;
        g.first_word = std::atoi(line.substr(t1 + 1, dash - t1 - 1).c_str());
        g.last_word = std::atoi(line.substr(dash + 1, t2 - dash - 1).c_str());
        std::istringstream units(line.substr(t2 + 1));
        for (std::string u; units >> u;)
            g.units.push_back(u);
        if (g.units.empty() || g.first_word < 1 || g.last_word < g.first_word) {
            *error = path + " line " + std::to_string(number) + ": bad group";
            return false;
        }
        seen[static_cast<size_t>(g.ayah - ayah_first)] = true;
        out->push_back(std::move(g));
    }
    for (size_t i = 0; i < seen.size(); ++i) {
        if (!seen[i]) {
            *error = "no expected units for " + std::to_string(surah) + ":" +
                     std::to_string(ayah_first + static_cast<int>(i));
            return false;
        }
    }
    return true;
}

}  // namespace mutqin
