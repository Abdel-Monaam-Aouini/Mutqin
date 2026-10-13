// C API: engine and session handles. Exceptions never cross the boundary.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

#include "mutqin.h"
#include "tajweed/phonemizer.h"
#include "text/quran.h"

struct mq_engine {
    mutqin::Quran quran;
};

struct mq_session {
    mutqin::PhonemizeResult result;
    std::vector<std::string> rules;  // joined rule tags, one per item
};

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
        for (int a = ayah_first; a <= ayah_last; ++a) {
            const mutqin::Ayah* found = engine->quran.FindAyah(surah, a);
            if (found == nullptr) {
                SetError(err, err_len, "no such ayah " + std::to_string(surah) + ":" + std::to_string(a));
                return MQ_ERR_ARGUMENT;
            }
            ayahs.push_back(found);
        }
        auto session = new mq_session();
        std::string error;
        if (!mutqin::PhonemizeRange(ayahs, &session->result, &error)) {
            delete session;
            SetError(err, err_len, error);
            return MQ_ERR_DATA;
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

}  // extern "C"
