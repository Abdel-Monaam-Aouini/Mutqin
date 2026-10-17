#include "align/locator.h"

#include <algorithm>
#include <map>

namespace mutqin {

uint64_t Locator::Key(const int* units) const {
    uint64_t key = 1469598103934665603ULL;
    for (int i = 0; i < params_.gram; ++i)
        key = (key ^ static_cast<uint64_t>(units[i] + 1)) * 1099511628211ULL;
    return key;
}

Locator::Locator(const std::vector<UnitGroup>& groups, int collapse_runs, const LocatorParams& params)
    : params_(params) {
    for (const UnitGroup& g : groups) {
        group_start_.push_back(flat_.size());
        group_place_.push_back({g.surah, g.ayah});
        for (const std::string& unit : g.units) {
            const auto it = ids_.emplace(CollapseRuns(unit, collapse_runs), static_cast<int>(ids_.size()));
            flat_.push_back(it.first->second);
        }
        if (g.surah == 1 && g.ayah == 1)
            first_after_basmala_ = flat_.size();
    }
    const size_t k = static_cast<size_t>(params_.gram);
    if (flat_.size() < k)
        return;
    // The basmala opens most recitations and occurs in more than one place (the first ayah of al-Fatihah and
    // 27:30), so its runs say nothing about where the recitation is.
    for (size_t p = 0; p + k <= first_after_basmala_; ++p)
        basmala_.insert(Key(&flat_[p]));
    for (size_t p = 0; p + k <= flat_.size(); ++p) {
        const uint64_t key = Key(&flat_[p]);
        if (basmala_.count(key) == 0)
            index_[key].push_back(static_cast<uint32_t>(p));
    }
}

LocateResult Locator::Locate(const std::vector<HeardToken>& heard) const {
    LocateResult result;
    std::vector<int> units;
    for (const HeardToken& t : heard) {
        const auto it = ids_.find(t.symbol);
        units.push_back(it == ids_.end() ? -1 : it->second);  // an unknown unit matches nothing
    }
    const size_t k = static_cast<size_t>(params_.gram);
    if (static_cast<int>(units.size()) < params_.min_heard || units.size() < k)
        return result;

    struct Bin {
        int votes = 0;
        std::vector<size_t> places;  // the places (flat_ index) that voted
    };
    std::map<long, Bin> bins;
    for (size_t i = 0; i + k <= units.size(); ++i) {
        if (std::find(units.begin() + static_cast<std::ptrdiff_t>(i),
                      units.begin() + static_cast<std::ptrdiff_t>(i + k),
                      -1) != units.begin() + static_cast<std::ptrdiff_t>(i + k))
            continue;
        const auto it = index_.find(Key(&units[i]));
        if (it == index_.end() || static_cast<int>(it->second.size()) > params_.max_occurrences)
            continue;
        for (uint32_t p : it->second) {
            Bin& bin = bins[(static_cast<long>(p) - static_cast<long>(i)) / params_.bin];
            ++bin.votes;
            bin.places.push_back(p);
        }
    }

    // a place is a bin with its two neighbours (a few units of insertions or deletions shift the diagonal)
    struct Candidate {
        long bin;
        int votes;
        std::vector<size_t> places;
    };
    std::vector<Candidate> candidates;
    for (const auto& [b, bin] : bins) {
        Candidate c{b, bin.votes, bin.places};
        for (long nb : {b - 1, b + 1}) {
            const auto it = bins.find(nb);
            if (it == bins.end())
                continue;
            c.votes += it->second.votes;
            c.places.insert(c.places.end(), it->second.places.begin(), it->second.places.end());
        }
        candidates.push_back(c);
    }
    if (candidates.empty())
        return result;
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.votes > b.votes; });
    const Candidate& best = candidates.front();
    if (best.votes < params_.min_votes)
        return result;
    for (size_t i = 1; i < candidates.size(); ++i) {
        if (std::abs(candidates[i].bin - best.bin) <= 3)
            continue;  // the same place
        if (static_cast<double>(best.votes) < params_.lead * candidates[i].votes)
            return result;  // another place agrees almost as well (the same words elsewhere): wait
        break;
    }
    // Where the recitation begins: the first place with company. A single stray vote just before the real
    // start (a chance match, or two) must not move it, so a place counts once two more votes follow within
    // a stretch of gram + 4 units: the votes of a real recitation come in runs.
    std::vector<size_t> places = best.places;
    std::sort(places.begin(), places.end());
    size_t start = places.front();
    for (size_t i = 0; i + 2 < places.size(); ++i) {
        if (places[i + 2] - places[i] <= static_cast<size_t>(params_.gram + 4)) {
            start = places[i];
            break;
        }
    }
    const size_t group =
        static_cast<size_t>(std::upper_bound(group_start_.begin(), group_start_.end(), start) -
                            group_start_.begin()) -
        1;
    result.found = true;
    result.surah = group_place_[group].surah;
    result.ayah = group_place_[group].ayah;
    // al-Fatihah is recited from its first ayah, the basmala, which is not voted with (see above)
    if (result.surah == 1)
        result.ayah = 1;
    result.votes = best.votes;
    return result;
}

}  // namespace mutqin
