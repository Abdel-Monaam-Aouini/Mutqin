// C API: engine and session handles. Exceptions never cross the boundary.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <mutex>
#include <string>
#include <tuple>
#include <vector>

#include "align/aligner.h"
#include "align/ctc.h"
#include "api/session_internal.h"
#include "audio/resampler.h"
#include "features/logmel.h"
#include "inference/sherpa_recognizer.h"
#include "inference/units.h"
#include "mutqin.h"
#include "tajweed/phonemizer.h"
#include "text/quran.h"

struct mq_engine {
    mutqin::Quran quran;
};

struct mq_session {
    const mq_engine* engine = nullptr;  // for the text of the items; must outlive the session
    mutqin::PhonemizeResult result;
    std::vector<std::string> rules;       // joined rule tags, one per item
    std::vector<std::string> item_rules;  // unique expected rule tags, one per recitation item
    std::vector<mutqin::WordRef> words;
    std::vector<mutqin::Frame> frames;
    mutqin::AlignResult aligned;
    std::unique_ptr<mutqin::StreamingAligner> aligner;
    mutqin::LogMel logmel;
    std::unique_ptr<mutqin::AcousticModel> model;
    std::unique_ptr<mutqin::TokenRecognizer> recognizer;
    int surah = 0, ayah_first = 0, ayah_last = 0;
    mutqin::AlignParams params;
    bool streaming = false;                        // audio is being pushed and not finished yet
    std::unique_ptr<mutqin::Resampler> resampler;  // null at 16 kHz
    std::vector<float> converted;                  // resampled audio of the current push
    mutable std::mutex mutex;                      // serialises the calls on this session
    int strictness = MQ_STRICTNESS_STANDARD;
    bool units_active = false;  // expected units from the model: the levels apply
    std::vector<mutqin::HeardToken>
        heard;  // the hypothesis so far (it can be judged again after a level change)

    // A long range is aligned through a sliding window of items [win_lo, win_hi): the aligner only holds
    // the words around the reciter's position, and what the window has left behind is final (done_*).
    // `aligned` always holds the merged result in item numbers of the whole range.
    size_t total_units = 0;  // expected units of the whole range
    size_t win_lo = 0, win_hi = 0;
    size_t heard_base = 0;  // index in the heard list where the window's first word starts
    std::map<int, std::string> done_statuses;
    std::vector<mutqin::AlignEvent> done_events;
    bool has_done_span = false;
    int done_span_first = 0;
};

namespace mutqin {
void SetSessionModel(mq_session* session, std::unique_ptr<AcousticModel> model) {
    session->model = std::move(model);
}
void SetSessionRecognizer(mq_session* session, std::unique_ptr<TokenRecognizer> recognizer) {
    session->recognizer = std::move(recognizer);
}
}  // namespace mutqin

namespace {

void SetError(char* err, size_t err_len, const std::string& message) {
    if (err == nullptr || err_len == 0)
        return;
    const size_t n = std::min(message.size(), err_len - 1);
    std::memcpy(err, message.data(), n);
    err[n] = '\0';
}

std::string Join(const std::vector<std::string>& parts) {
    std::string out;
    for (const std::string& p : parts) {
        if (!out.empty())
            out += ',';
        out += p;
    }
    return out;
}

std::string JoinUniqueRules(const std::vector<const mutqin::PhonemeItem*>& ayah_items,
                            const mutqin::WordRef& word) {
    const int first = std::get<2>(word.key);
    const int last = word.last_word;
    std::vector<std::string> unique;
    for (const mutqin::PhonemeItem* item : ayah_items) {
        if (item->word < first || item->word > last)
            continue;
        for (const std::string& rule : item->rules) {
            if (std::find(unique.begin(), unique.end(), rule) == unique.end())
                unique.push_back(rule);
        }
    }
    return Join(unique);
}

void RebuildItemRules(mq_session* session) {
    // the items of each ayah once, so that a whole surah is not a words x items search
    std::map<std::pair<int, int>, std::vector<const mutqin::PhonemeItem*>> by_ayah;
    for (const mutqin::PhonemeItem& item : session->result.items)
        by_ayah[{item.surah, item.ayah}].push_back(&item);
    static const std::vector<const mutqin::PhonemeItem*> none;
    session->item_rules.clear();
    session->item_rules.reserve(session->words.size());
    for (const mutqin::WordRef& word : session->words) {
        const auto it = by_ayah.find({std::get<0>(word.key), std::get<1>(word.key)});
        session->item_rules.push_back(JoinUniqueRules(it == by_ayah.end() ? none : it->second, word));
    }
}

}  // namespace

namespace {
// Every public call on a session holds its lock, so one thread can push audio while another polls events.
class SessionLock {
public:
    explicit SessionLock(const mq_session* session) {
        if (session != nullptr)
            lock_ = std::unique_lock<std::mutex>(session->mutex);
    }

private:
    std::unique_lock<std::mutex> lock_;
};
}  // namespace

namespace {
// While audio streams, the last word heard so far is only partly in and is not judged: events and
// statuses about it would flash and then disappear. Once the audio is finished everything is judged.
bool Settled(const mq_session* session, int item) {
    return !session->streaming || !session->aligned.has_span || item < session->aligned.span_last;
}

std::vector<size_t> VisibleEvents(const mq_session* session) {
    std::vector<size_t> visible;
    for (size_t i = 0; i < session->aligned.events.size(); ++i)
        if (Settled(session, session->aligned.events[i].index))
            visible.push_back(i);
    return visible;
}
}  // namespace

extern "C" {

int mq_engine_create(const char* data_dir, mq_engine** out, char* err, size_t err_len) {
    if (out == nullptr)
        return MQ_ERR_ARGUMENT;
    *out = nullptr;
    if (data_dir == nullptr) {
        SetError(err, err_len, "data_dir is null");
        return MQ_ERR_ARGUMENT;
    }
    try {
        auto engine = new mq_engine();
        std::string error;
        if (!mutqin::Quran::Load(data_dir, &engine->quran, &error)) {
            delete engine;
            SetError(err, err_len, error);
            return MQ_ERR_DATA;
        }
        *out = engine;
        return MQ_OK;
    } catch (const std::exception& e) {
        SetError(err, err_len, e.what());
        return MQ_ERR_INTERNAL;
    }
}

int mq_engine_create_from_memory(const char* uthmani, size_t uthmani_len, const char* simple,
                                 size_t simple_len, const char* checksums, size_t checksums_len,
                                 mq_engine** out, char* err, size_t err_len) {
    if (out == nullptr)
        return MQ_ERR_ARGUMENT;
    *out = nullptr;
    if (uthmani == nullptr || simple == nullptr || checksums == nullptr) {
        SetError(err, err_len, "a data pointer is null");
        return MQ_ERR_ARGUMENT;
    }
    try {
        mutqin::QuranSources sources;
        sources.uthmani.assign(uthmani, uthmani_len);
        sources.simple.assign(simple, simple_len);
        sources.checksums.assign(checksums, checksums_len);
        auto engine = new mq_engine();
        std::string error;
        if (!mutqin::Quran::LoadFromText(sources, &engine->quran, &error)) {
            delete engine;
            SetError(err, err_len, error);
            return MQ_ERR_DATA;
        }
        *out = engine;
        return MQ_OK;
    } catch (const std::exception& e) {
        SetError(err, err_len, e.what());
        return MQ_ERR_INTERNAL;
    }
}

int mq_engine_ayah_count(const mq_engine* engine, int surah) {
    if (engine == nullptr || surah < 1 || surah > static_cast<int>(engine->quran.surahs().size()))
        return -1;
    return static_cast<int>(engine->quran.surahs()[static_cast<size_t>(surah - 1)].ayahs.size());
}

void mq_engine_destroy(mq_engine* engine) {
    delete engine;
}

int mq_session_create(const mq_engine* engine, int surah, int ayah_first, int ayah_last, mq_session** out,
                      char* err, size_t err_len) {
    if (out == nullptr)
        return MQ_ERR_ARGUMENT;
    *out = nullptr;
    if (engine == nullptr || ayah_first < 1 || ayah_last < ayah_first) {
        SetError(err, err_len, "invalid engine or ayah range");
        return MQ_ERR_ARGUMENT;
    }
    try {
        std::vector<const mutqin::Ayah*> ayahs;
        std::map<std::pair<int, int>, int> ayah_words;
        for (int a = ayah_first; a <= ayah_last; ++a) {
            const mutqin::Ayah* found = engine->quran.FindAyah(surah, a);
            if (found == nullptr) {
                SetError(err, err_len, "no such ayah " + std::to_string(surah) + ":" + std::to_string(a));
                return MQ_ERR_ARGUMENT;
            }
            ayahs.push_back(found);
            ayah_words[{surah, a}] = static_cast<int>(found->words.size());
        }
        // Rules look at the next word, so the ayah after the range is phonemized too (and dropped).
        std::vector<const mutqin::Ayah*> with_next = ayahs;
        if (const mutqin::Ayah* next = engine->quran.FindAyah(surah, ayah_last + 1))
            with_next.push_back(next);
        auto session = new mq_session();
        std::string error;
        if (!mutqin::PhonemizeRange(with_next, &session->result, &error)) {
            delete session;
            SetError(err, err_len, error);
            return MQ_ERR_DATA;
        }
        auto& items = session->result.items;
        items.erase(std::remove_if(items.begin(), items.end(),
                                   [&](const mutqin::PhonemeItem& it) { return it.ayah > ayah_last; }),
                    items.end());
        session->words = mutqin::BuildWordReadings(items, ayah_words);
        session->engine = engine;
        session->surah = surah;
        session->ayah_first = ayah_first;
        session->ayah_last = ayah_last;
        size_t expected = 0;
        for (const mutqin::WordRef& w : session->words)
            expected += w.reading.cont.size();
        if (expected > MQ_MAX_EXPECTED_PHONEMES) {
            delete session;
            SetError(err, err_len,
                     "range too long (" + std::to_string(expected) + " phonemes, at most " +
                         std::to_string(MQ_MAX_EXPECTED_PHONEMES) + ")");
            return MQ_ERR_LIMIT;
        }
        for (const mutqin::PhonemeItem& item : session->result.items)
            session->rules.push_back(Join(item.rules));
        RebuildItemRules(session);
        *out = session;
        return MQ_OK;
    } catch (const std::exception& e) {
        SetError(err, err_len, e.what());
        return MQ_ERR_INTERNAL;
    }
}

void mq_session_destroy(mq_session* session) {
    delete session;
}

size_t mq_session_phoneme_count(const mq_session* session) {
    SessionLock lock(session);
    return session == nullptr ? 0 : session->result.items.size();
}

int mq_session_phoneme(const mq_session* session, size_t index, mq_phoneme* out) {
    SessionLock lock(session);
    if (session == nullptr || out == nullptr || index >= session->result.items.size())
        return MQ_ERR_ARGUMENT;
    const mutqin::PhonemeItem& item = session->result.items[index];
    out->symbol = item.ph.c_str();
    out->surah = item.surah;
    out->ayah = item.ayah;
    out->word = item.word;
    out->variant = static_cast<int>(item.variant);
    out->rules = session->rules[index].c_str();
    out->dur_min = item.dur.empty() ? 0 : *std::min_element(item.dur.begin(), item.dur.end());
    out->dur_max = item.dur.empty() ? 0 : *std::max_element(item.dur.begin(), item.dur.end());
    return MQ_OK;
}

namespace {
// Ranges up to kSingleWindowUnits expected units are aligned as a whole. A longer range slides a window of
// about kWindowUnits units over the words: once fewer than kSlideAheadUnits units are left ahead of the
// reciter, the window moves on, keeping the last kBackWords words so that a repeat can still be followed.
constexpr size_t kSingleWindowUnits = 3000;
constexpr size_t kWindowUnits = 2000;
constexpr size_t kSlideAheadUnits = 500;
constexpr size_t kBackWords = 40;

size_t UnitsBetween(const mq_session* session, size_t from, size_t to) {
    size_t units = 0;
    for (size_t i = from; i < to; ++i)
        units += session->words[i].reading.cont.size();
    return units;
}

size_t WindowEnd(const mq_session* session, size_t lo) {
    const size_t n = session->words.size();
    if (UnitsBetween(session, lo, n) <= kSingleWindowUnits)
        return n;
    size_t hi = lo + 1, units = session->words[lo].reading.cont.size();
    while (hi < n && units < kWindowUnits)
        units += session->words[hi++].reading.cont.size();
    return hi;
}

void BuildAligner(mq_session* session) {
    std::vector<mutqin::WordReading> readings;
    for (size_t i = session->win_lo; i < session->win_hi; ++i)
        readings.push_back(session->words[i].reading);
    session->aligner = std::make_unique<mutqin::StreamingAligner>(std::move(readings), session->params);
}

// Starts the alignment of the whole range from the first word (after a reset, a new model or level).
void EnsureAligner(mq_session* session) {
    if (session->aligner)
        return;
    session->total_units = UnitsBetween(session, 0, session->words.size());
    session->win_lo = 0;
    session->win_hi = WindowEnd(session, 0);
    session->heard_base = 0;
    session->done_statuses.clear();
    session->done_events.clear();
    session->has_done_span = false;
    BuildAligner(session);
}

// `aligned` = what the window left behind + the window's own result, in item numbers of the whole range.
void MergeWindow(mq_session* session, const mutqin::AlignResult& local) {
    const int lo = static_cast<int>(session->win_lo);
    mutqin::AlignResult& out = session->aligned;
    out = mutqin::AlignResult();
    out.statuses = session->done_statuses;
    for (const auto& [w, status] : local.statuses)
        out.statuses[w + lo] = status;
    out.events = session->done_events;
    for (mutqin::AlignEvent e : local.events) {
        e.index += lo;
        out.events.push_back(e);
    }
    if (local.has_span) {
        out.has_span = true;
        out.span_first = session->has_done_span ? session->done_span_first : local.span_first + lo;
        out.span_last = local.span_last + lo;
    } else if (session->has_done_span) {
        out.has_span = true;
        out.span_first = session->done_span_first;
        out.span_last = lo;
    }
}

// Moves the window on if the reciter is close to its end. The words left behind keep the verdicts they have.
// False if the window stays where it is.
bool SlideWindow(mq_session* session, const mutqin::AlignResult& local) {
    if (!local.has_span || session->win_hi >= session->words.size())
        return false;
    const size_t lo = session->win_lo;
    const size_t position = lo + static_cast<size_t>(local.span_last);
    if (UnitsBetween(session, position, session->win_hi) >= kSlideAheadUnits)
        return false;
    // the new first word: kBackWords before the reciter, the first of those that was heard
    const int want = static_cast<int>(position > lo + kBackWords ? position - lo - kBackWords : 0);
    const auto first = local.first_heard.lower_bound(want);
    if (first == local.first_heard.end() || first->first <= 0)
        return false;
    const int rel = first->first;
    if (!session->has_done_span) {
        session->has_done_span = true;
        session->done_span_first = static_cast<int>(lo) + local.span_first;
    }
    for (const auto& [w, status] : local.statuses)
        if (w < rel)
            session->done_statuses[static_cast<int>(lo) + w] = status;
    for (mutqin::AlignEvent e : local.events) {
        if (e.index < rel) {
            e.index += static_cast<int>(lo);
            session->done_events.push_back(e);
        }
    }
    session->heard_base += static_cast<size_t>(first->second);
    session->win_lo = lo + static_cast<size_t>(rel);
    session->win_hi = WindowEnd(session, session->win_lo);
    BuildAligner(session);
    return true;
}

// Aligns what has been heard so far (session->heard). False if more was heard than the session may hold
// (memory bound, see mutqin.h).
bool AlignCurrent(mq_session* session) {
    EnsureAligner(session);
    const std::vector<mutqin::HeardToken>& heard = session->heard;
    if (heard.size() > 2 * session->total_units + 500)
        return false;
    // A replay of a long recitation (new level) may move the window several times in one call.
    for (;;) {
        const size_t base = std::min(session->heard_base, heard.size());
        const std::vector<mutqin::HeardToken> local_heard(heard.begin() + static_cast<std::ptrdiff_t>(base),
                                                          heard.end());
        if (local_heard.size() > 2 * UnitsBetween(session, session->win_lo, session->win_hi) + 500)
            return false;
        const mutqin::AlignResult local = session->aligner->Update(local_heard);
        MergeWindow(session, local);
        if (!SlideWindow(session, local))
            return true;
    }
}

// The hypothesis changed from token number `from` on (the tokens before it are final): align again.
bool AlignHeard(mq_session* session, size_t from, const std::vector<mutqin::HeardToken>& tail) {
    from = std::min(from, session->heard.size());
    session->heard.resize(from);
    session->heard.insert(session->heard.end(), tail.begin(), tail.end());
    return AlignCurrent(session);
}

bool Realign(mq_session* session, const float* probs, size_t num_frames) {
    for (size_t i = 0; i < num_frames; ++i)
        session->frames.push_back(mutqin::ArgmaxFrame(probs + i * MQ_VOCAB_SIZE));
    return AlignHeard(session, 0, mutqin::GreedyDecode(session->frames));
}
}  // namespace

int mq_session_push_posteriors(mq_session* session, const float* probs, size_t num_frames) {
    SessionLock lock(session);
    if (session == nullptr || (probs == nullptr && num_frames > 0))
        return MQ_ERR_ARGUMENT;
    try {
        return Realign(session, probs, num_frames) ? MQ_OK : MQ_ERR_LIMIT;
    } catch (const std::exception&) {
        return MQ_ERR_INTERNAL;
    }
}

int mq_session_set_input_rate(mq_session* session, int sample_rate_hz) {
    SessionLock lock(session);
    if (session == nullptr || sample_rate_hz < 8000 || sample_rate_hz > 192000)
        return MQ_ERR_ARGUMENT;
    try {
        if (sample_rate_hz == 16000)
            session->resampler.reset();
        else
            session->resampler = std::make_unique<mutqin::Resampler>(sample_rate_hz);
        return MQ_OK;
    } catch (const std::exception&) {
        return MQ_ERR_INTERNAL;
    }
}

int mq_session_push_audio(mq_session* session, const float* samples, size_t num_samples) {
    SessionLock lock(session);
    if (session == nullptr || (samples == nullptr && num_samples > 0))
        return MQ_ERR_ARGUMENT;
    if (session->resampler) {
        try {
            session->converted.clear();
            session->resampler->Accept(samples, num_samples, &session->converted);
        } catch (const std::exception&) {
            return MQ_ERR_INTERNAL;
        }
        samples = session->converted.data();
        num_samples = session->converted.size();
    }
    session->streaming = true;
    if (session->recognizer) {
        try {
            std::vector<mutqin::HeardToken> tail;
            size_t from = 0;
            session->recognizer->Accept(samples, num_samples, &tail, &from);
            return AlignHeard(session, from, tail) ? MQ_OK : MQ_ERR_LIMIT;
        } catch (const std::exception&) {
            return MQ_ERR_INTERNAL;
        }
    }
    if (!session->model)
        return MQ_ERR_NO_MODEL;
    try {
        const std::vector<float> feats = session->logmel.Accept(samples, num_samples);
        std::vector<float> posteriors;
        session->model->Accept(feats.data(), feats.size() / 80, &posteriors);
        if (!posteriors.empty() && !Realign(session, posteriors.data(), posteriors.size() / MQ_VOCAB_SIZE))
            return MQ_ERR_LIMIT;
        return MQ_OK;
    } catch (const std::exception&) {
        return MQ_ERR_INTERNAL;
    }
}

int mq_session_finish_audio(mq_session* session) {
    SessionLock lock(session);
    if (session == nullptr)
        return MQ_ERR_ARGUMENT;
    if (!session->recognizer) {
        session->streaming = false;
        return session->model ? MQ_OK : MQ_ERR_NO_MODEL;
    }
    try {
        std::vector<mutqin::HeardToken> tail;
        size_t from = 0;
        session->recognizer->Finish(&tail, &from);
        session->streaming = false;
        return AlignHeard(session, from, tail) ? MQ_OK : MQ_ERR_LIMIT;
    } catch (const std::exception&) {
        return MQ_ERR_INTERNAL;
    }
}

int mq_session_load_model(mq_session* session, const char* manifest_path, char* err, size_t err_len) {
    SessionLock lock(session);
    if (session == nullptr || manifest_path == nullptr)
        return MQ_ERR_ARGUMENT;
#ifdef MUTQIN_HAVE_SHERPA
    try {
        mutqin::SherpaConfig config;
        std::string error;
        if (!mutqin::ReadModelManifest(manifest_path, &config, &error)) {
            SetError(err, err_len, error);
            return MQ_ERR_DATA;
        }
        std::unique_ptr<mutqin::TokenRecognizer> recognizer = mutqin::CreateSherpaRecognizer(config, &error);
        if (!recognizer) {
            SetError(err, err_len, error);
            return MQ_ERR_DATA;
        }
        if (!config.units.empty()) {
            // The model's own reading: expected units per group of words, compared as they are.
            std::vector<mutqin::UnitGroup> groups;
            if (!mutqin::LoadUnitGroups(config.units, session->surah, session->ayah_first, session->ayah_last,
                                        &groups, &error)) {
                SetError(err, err_len, error);
                return MQ_ERR_DATA;
            }
            // A recitation of a surah usually opens with the basmala, which is not a word of its first ayah.
            // Its groups (those of 1:1) are put in front as ayah 0; if the reciter leaves it out the
            // aligner's free start skips them.
            if (session->ayah_first == 1 && session->surah != 1 && session->surah != 9) {
                std::vector<mutqin::UnitGroup> basmala;
                if (!mutqin::LoadUnitGroups(config.units, 1, 1, 1, &basmala, &error)) {
                    SetError(err, err_len, error);
                    return MQ_ERR_DATA;
                }
                for (mutqin::UnitGroup& g : basmala) {
                    g.surah = session->surah;
                    g.ayah = 0;
                }
                groups.insert(groups.begin(), basmala.begin(), basmala.end());
            }
            std::vector<mutqin::WordRef> words;
            size_t expected = 0;
            for (const mutqin::UnitGroup& g : groups) {
                mutqin::WordRef w;
                w.key = mutqin::WordKey(g.surah, g.ayah, g.first_word);
                w.reading.ayah = g.ayah;
                w.last_word = g.last_word;
                for (const std::string& u : g.units)
                    w.reading.cont.push_back(mutqin::CollapseRuns(u, config.collapse_runs));
                for (auto& context : w.reading.contexts)
                    context = w.reading.cont;
                expected += w.reading.cont.size();
                words.push_back(std::move(w));
            }
            if (expected > MQ_MAX_EXPECTED_PHONEMES) {
                SetError(err, err_len, "range too long for the model's units");
                return MQ_ERR_LIMIT;
            }
            session->words = std::move(words);
            session->units_active = true;
            session->params = mutqin::StrictnessProfile(session->strictness);  // tuned on real model output
            session->aligner.reset();
            session->aligned = mutqin::AlignResult();
            session->frames.clear();
            session->heard.clear();
            RebuildItemRules(session);
        }
        session->recognizer = std::move(recognizer);
        return MQ_OK;
    } catch (const std::exception& e) {
        SetError(err, err_len, e.what());
        return MQ_ERR_INTERNAL;
    }
#else
    SetError(err, err_len, "this build has no inference backend (configure with -DMUTQIN_WITH_SHERPA=ON)");
    return MQ_ERR_NO_MODEL;
#endif
}

int mq_session_item(const mq_session* session, size_t item_index, mq_item* out) {
    SessionLock lock(session);
    if (session == nullptr || out == nullptr || item_index >= session->words.size())
        return MQ_ERR_ARGUMENT;
    const mutqin::WordRef& w = session->words[item_index];
    out->surah = std::get<0>(w.key);
    out->ayah = std::get<1>(w.key);
    out->word = std::get<2>(w.key);
    out->word_last = w.last_word;
    return MQ_OK;
}

int mq_session_item_rules(const mq_session* session, size_t item_index, char* out, size_t out_len) {
    SessionLock lock(session);
    if (session == nullptr || item_index >= session->words.size())
        return MQ_ERR_ARGUMENT;
    const std::string& rules = session->item_rules[item_index];
    if (out != nullptr && out_len > 0) {
        const size_t n = std::min(rules.size(), out_len - 1);
        std::memcpy(out, rules.data(), n);
        out[n] = '\0';
    }
    return static_cast<int>(rules.size());
}

int mq_session_item_text(const mq_session* session, size_t item_index, char* out, size_t out_len) {
    SessionLock lock(session);
    if (session == nullptr || session->engine == nullptr || item_index >= session->words.size())
        return MQ_ERR_ARGUMENT;
    const mutqin::WordRef& item = session->words[item_index];
    const int surah_number = std::get<0>(item.key), ayah_number = std::get<1>(item.key);
    const int first = std::get<2>(item.key), last = item.last_word;
    const auto& surahs = session->engine->quran.surahs();
    if (surah_number < 1 || surah_number > static_cast<int>(surahs.size()))
        return MQ_ERR_INTERNAL;
    const mutqin::Surah& surah = surahs[static_cast<size_t>(surah_number - 1)];
    std::string text;
    auto append = [&text](const std::string& piece) {
        if (!text.empty())
            text += ' ';
        text += piece;
    };
    if (ayah_number == 0) {  // the basmala preface
        for (int w = first; w <= last && w <= static_cast<int>(surah.basmala.size()); ++w)
            append(surah.basmala[static_cast<size_t>(w - 1)]);
    } else {
        const mutqin::Ayah* ayah = session->engine->quran.FindAyah(surah_number, ayah_number);
        if (ayah == nullptr)
            return MQ_ERR_INTERNAL;
        for (int w = first; w <= last && w <= static_cast<int>(ayah->words.size()); ++w) {
            const mutqin::Word& word = ayah->words[static_cast<size_t>(w - 1)];
            append(word.text);
            for (const std::string& mark : word.marks)
                append(mark);
        }
    }
    if (out != nullptr && out_len > 0) {
        const size_t n = std::min(text.size(), out_len - 1);
        std::memcpy(out, text.data(), n);
        out[n] = '\0';
    }
    return static_cast<int>(text.size());
}

int mq_session_position(const mq_session* session) {
    SessionLock lock(session);
    if (session == nullptr || !session->aligned.has_span)
        return -1;
    return session->aligned.span_last;
}

int mq_session_set_strictness(mq_session* session, int level) {
    SessionLock lock(session);
    if (session == nullptr || level < MQ_STRICTNESS_RELAXED || level > MQ_STRICTNESS_STRICT)
        return MQ_ERR_ARGUMENT;
    try {
        session->strictness = level;
        if (!session->units_active)
            return MQ_OK;  // remembered for when units are loaded
        session->params = mutqin::StrictnessProfile(level);
        session->aligner.reset();  // the stored alignment belongs to the old parameters
        if (!session->heard.empty())
            AlignCurrent(session);  // judge what was heard so far again
        return MQ_OK;
    } catch (const std::exception&) {
        return MQ_ERR_INTERNAL;
    }
}

int mq_session_reset(mq_session* session) {
    SessionLock lock(session);
    if (session == nullptr)
        return MQ_ERR_ARGUMENT;
    try {
        session->frames.clear();
        session->heard.clear();
        session->aligned = mutqin::AlignResult();
        session->aligner.reset();
        session->logmel = mutqin::LogMel();
        session->converted.clear();
        session->streaming = false;
        if (session->resampler)
            session->resampler->Reset();
        if (session->model)
            session->model->Reset();
        if (session->recognizer)
            session->recognizer->Reset();
        return MQ_OK;
    } catch (const std::exception&) {
        return MQ_ERR_INTERNAL;
    }
}

size_t mq_session_word_count(const mq_session* session) {
    SessionLock lock(session);
    return session == nullptr ? 0 : session->words.size();
}

int mq_session_word_status(const mq_session* session, size_t word_index) {
    SessionLock lock(session);
    if (session == nullptr || word_index >= session->words.size())
        return -1;
    const auto it = session->aligned.statuses.find(static_cast<int>(word_index));
    if (it == session->aligned.statuses.end() || !Settled(session, static_cast<int>(word_index)))
        return MQ_WORD_NOT_JUDGED;
    if (it->second == "wrong")
        return MQ_WORD_WRONG;
    if (it->second == "skipped")
        return MQ_WORD_SKIPPED;
    return MQ_WORD_OK;
}

size_t mq_session_event_count(const mq_session* session) {
    SessionLock lock(session);
    return session == nullptr ? 0 : VisibleEvents(session).size();
}

int mq_session_event(const mq_session* session, size_t index, mq_event* out) {
    SessionLock lock(session);
    if (session == nullptr || out == nullptr)
        return MQ_ERR_ARGUMENT;
    const std::vector<size_t> visible = VisibleEvents(session);
    if (index >= visible.size())
        return MQ_ERR_ARGUMENT;
    const mutqin::AlignEvent& e = session->aligned.events[visible[index]];
    const mutqin::WordKey& key = session->words[static_cast<size_t>(e.index)].key;
    out->kind = e.kind == "skipped"    ? MQ_EVENT_SKIPPED
                : e.kind == "wrong"    ? MQ_EVENT_WRONG
                : e.kind == "repeated" ? MQ_EVENT_REPEATED
                                       : MQ_EVENT_ADDED;
    out->surah = std::get<0>(key);
    out->ayah = std::get<1>(key);
    out->word = std::get<2>(key);
    out->word_index = static_cast<size_t>(e.index);
    out->word_last = session->words[static_cast<size_t>(e.index)].last_word;
    out->confidence = e.confidence;
    return MQ_OK;
}

}  // extern "C"
