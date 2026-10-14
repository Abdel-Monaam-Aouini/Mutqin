// C API: engine and session handles. Exceptions never cross the boundary.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <map>
#include <string>
#include <tuple>
#include <vector>

#include "align/aligner.h"
#include "align/ctc.h"
#include "api/session_internal.h"
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
    mutqin::PhonemizeResult result;
    std::vector<std::string> rules;  // joined rule tags, one per item
    std::vector<mutqin::WordRef> words;
    std::vector<mutqin::Frame> frames;
    mutqin::AlignResult aligned;
    std::unique_ptr<mutqin::StreamingAligner> aligner;
    mutqin::LogMel logmel;
    std::unique_ptr<mutqin::AcousticModel> model;
    std::unique_ptr<mutqin::TokenRecognizer> recognizer;
    int surah = 0, ayah_first = 0, ayah_last = 0;
    mutqin::AlignParams params;
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
    return session == nullptr ? 0 : session->result.items.size();
}

int mq_session_phoneme(const mq_session* session, size_t index, mq_phoneme* out) {
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
void EnsureAligner(mq_session* session) {
    if (session->aligner)
        return;
    std::vector<mutqin::WordReading> readings;
    for (const mutqin::WordRef& w : session->words)
        readings.push_back(w.reading);
    session->aligner = std::make_unique<mutqin::StreamingAligner>(std::move(readings), session->params);
}

// False if more was heard than the session may hold (memory bound, see mutqin.h).
bool AlignHeard(mq_session* session, const std::vector<mutqin::HeardToken>& heard) {
    size_t expected = 0;
    for (const mutqin::WordRef& w : session->words)
        expected += w.reading.cont.size();
    if (heard.size() > 2 * expected + 500)
        return false;
    EnsureAligner(session);
    session->aligned = session->aligner->Update(heard);
    return true;
}

bool Realign(mq_session* session, const float* probs, size_t num_frames) {
    for (size_t i = 0; i < num_frames; ++i)
        session->frames.push_back(mutqin::ArgmaxFrame(probs + i * MQ_VOCAB_SIZE));
    return AlignHeard(session, mutqin::GreedyDecode(session->frames));
}
}  // namespace

int mq_session_push_posteriors(mq_session* session, const float* probs, size_t num_frames) {
    if (session == nullptr || (probs == nullptr && num_frames > 0))
        return MQ_ERR_ARGUMENT;
    try {
        return Realign(session, probs, num_frames) ? MQ_OK : MQ_ERR_LIMIT;
    } catch (const std::exception&) {
        return MQ_ERR_INTERNAL;
    }
}

int mq_session_push_audio(mq_session* session, const float* samples, size_t num_samples) {
    if (session == nullptr || (samples == nullptr && num_samples > 0))
        return MQ_ERR_ARGUMENT;
    if (session->recognizer) {
        try {
            std::vector<mutqin::HeardToken> heard;
            session->recognizer->Accept(samples, num_samples, &heard);
            return AlignHeard(session, heard) ? MQ_OK : MQ_ERR_LIMIT;
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
    if (session == nullptr)
        return MQ_ERR_ARGUMENT;
    if (!session->recognizer)
        return session->model ? MQ_OK : MQ_ERR_NO_MODEL;
    try {
        std::vector<mutqin::HeardToken> heard;
        session->recognizer->Finish(&heard);
        return AlignHeard(session, heard) ? MQ_OK : MQ_ERR_LIMIT;
    } catch (const std::exception&) {
        return MQ_ERR_INTERNAL;
    }
}

int mq_session_load_model(mq_session* session, const char* manifest_path, char* err, size_t err_len) {
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
            session->params.edge_free = 2;  // tuned on real model output (docs/EVAL_RESULTS.md)
            session->params.add_min_tokens = 3;
            session->aligner.reset();
            session->aligned = mutqin::AlignResult();
            session->frames.clear();
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

size_t mq_session_word_count(const mq_session* session) {
    return session == nullptr ? 0 : session->words.size();
}

int mq_session_word_status(const mq_session* session, size_t word_index) {
    if (session == nullptr || word_index >= session->words.size())
        return -1;
    const auto it = session->aligned.statuses.find(static_cast<int>(word_index));
    if (it == session->aligned.statuses.end())
        return MQ_WORD_NOT_JUDGED;
    if (it->second == "wrong")
        return MQ_WORD_WRONG;
    if (it->second == "skipped")
        return MQ_WORD_SKIPPED;
    return MQ_WORD_OK;
}

size_t mq_session_event_count(const mq_session* session) {
    return session == nullptr ? 0 : session->aligned.events.size();
}

int mq_session_event(const mq_session* session, size_t index, mq_event* out) {
    if (session == nullptr || out == nullptr || index >= session->aligned.events.size())
        return MQ_ERR_ARGUMENT;
    const mutqin::AlignEvent& e = session->aligned.events[index];
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
