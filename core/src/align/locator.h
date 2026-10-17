// Finds where in the Quran a recitation is, from the units heard so far and without knowing the surah or
// the ayah. The expected units of the whole Quran are indexed by short runs of units (k-grams); every run
// heard votes for the places it occurs, and a recitation puts its votes on one "diagonal" (the same offset
// between the heard units and the Quran's units). The place is reported once one diagonal clearly leads.
#ifndef MUTQIN_ALIGN_LOCATOR_H
#define MUTQIN_ALIGN_LOCATOR_H

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "align/aligner.h"
#include "inference/units.h"

namespace mutqin {

struct LocateResult {
    bool found = false;
    int surah = 0, ayah = 0;  // the first ayah the reciter has been heard in
    int votes = 0;            // k-grams heard that agree on the place
};

struct LocatorParams {
    int gram = 5;              // units per k-gram
    int min_heard = 14;        // units heard before a place is reported
    int min_votes = 5;         // k-grams that must agree
    double lead = 2.0;         // the best place needs this many times the votes of the next one
    int max_occurrences = 64;  // k-grams that occur in more places are too common to tell anything
    int bin = 12;              // diagonals closer than this (in units) are the same place
};

class Locator {
public:
    // `groups` are the groups of the whole Quran in order (units.h). Runs of one unit are cut to
    // `collapse_runs` as the recognizer does with the units it hears (0 = off).
    explicit Locator(const std::vector<UnitGroup>& groups, int collapse_runs,
                     const LocatorParams& params = LocatorParams());

    // `heard` is the hypothesis so far (the units as the recognizer returns them).
    LocateResult Locate(const std::vector<HeardToken>& heard) const;

private:
    struct Place {
        int surah, ayah;
    };
    uint64_t Key(const int* units) const;

    LocatorParams params_;
    std::unordered_map<std::string, int> ids_;
    std::vector<int> flat_;                 // the units of all groups, in order
    std::vector<size_t> group_start_;       // where each group starts in flat_
    std::vector<Place> group_place_;        // its ayah
    size_t first_after_basmala_ = 0;        // flat_ index where the group after 1:1 starts
    std::unordered_set<uint64_t> basmala_;  // k-grams of the basmala
    std::unordered_map<uint64_t, std::vector<uint32_t>> index_;
};

}  // namespace mutqin

#endif  // MUTQIN_ALIGN_LOCATOR_H
