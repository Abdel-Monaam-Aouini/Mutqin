#include "inference/sherpa_recognizer.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#include "inference/units.h"
#include "tajweed/inventory.h"

#ifdef MUTQIN_HAVE_SHERPA
#include "sherpa-onnx/c-api/c-api.h"
#endif

namespace mutqin {

bool ParseTokenMap(const std::string& text, TokenMap* out, std::string* error) {
    out->clear();
    std::istringstream in(text);
    std::string line;
    int number = 0;
    while (std::getline(in, line)) {
        ++number;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        const size_t tab = line.find('\t');
        if (tab == std::string::npos || tab == 0) {
            *error = "token map line " + std::to_string(number) + ": expected <unit><TAB><symbols>";
            return false;
        }
        std::vector<std::string> symbols;
        std::istringstream rest(line.substr(tab + 1));
        for (std::string s; rest >> s;) {
            if (TokenId(s) < 1) {
                *error = "token map line " + std::to_string(number) + ": unknown phoneme symbol '" + s + "'";
                return false;
            }
            symbols.push_back(s);
        }
        (*out)[line.substr(0, tab)] = symbols;
    }
    return true;
}

bool ReadModelManifest(const std::string& path, SherpaConfig* out, std::string* error) {
    std::ifstream in(path);
    if (!in) {
        *error = "cannot open " + path;
        return false;
    }
    std::string dir = ".";
    const size_t slash = path.find_last_of("/\\");
    if (slash != std::string::npos)
        dir = path.substr(0, slash);
    auto resolve = [&](const std::string& p) { return (!p.empty() && p[0] == '/') ? p : dir + "/" + p; };
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        if (line.empty() || line[0] == '#')
            continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) {
            *error = "manifest line without '=': " + line;
            return false;
        }
        const std::string key = line.substr(0, eq), value = line.substr(eq + 1);
        if (key == "model")
            out->model = resolve(value);
        else if (key == "tokens")
            out->tokens = resolve(value);
        else if (key == "token_map")
            out->token_map = resolve(value);
        else if (key == "units")
            out->units = resolve(value);
        else if (key == "collapse_runs")
            out->collapse_runs = std::max(0, std::atoi(value.c_str()));
        else if (key == "threads")
            out->threads = std::max(1, std::atoi(value.c_str()));
        else {
            *error = "unknown manifest key '" + key + "'";
            return false;
        }
    }
    if (out->model.empty() || out->tokens.empty() || (out->token_map.empty() && out->units.empty())) {
        *error = "manifest needs model=, tokens= and one of token_map= or units=";
        return false;
    }
    return true;
}

#ifdef MUTQIN_HAVE_SHERPA

namespace {

class SherpaRecognizer : public TokenRecognizer {
public:
    // `identity`: the model's units are used as they are (no token map), cut by `collapse` if > 0.
    SherpaRecognizer(const SherpaOnnxOnlineRecognizer* recognizer, TokenMap map, bool identity, int collapse)
        : recognizer_(recognizer),
          map_(std::move(map)),
          identity_(identity),
          collapse_(collapse),
          stream_(SherpaOnnxCreateOnlineStream(recognizer)) {}

    ~SherpaRecognizer() override {
        SherpaOnnxDestroyOnlineStream(stream_);
        SherpaOnnxDestroyOnlineRecognizer(recognizer_);
    }

    void Accept(const float* samples, size_t count, std::vector<HeardToken>* tokens) override {
        SherpaOnnxOnlineStreamAcceptWaveform(stream_, 16000, samples, static_cast<int32_t>(count));
        Decode(tokens);
    }

    void Finish(std::vector<HeardToken>* tokens) override {
        SherpaOnnxOnlineStreamInputFinished(stream_);
        Decode(tokens);
    }

    void Reset() override { SherpaOnnxOnlineStreamReset(recognizer_, stream_); }

private:
    void Decode(std::vector<HeardToken>* tokens) {
        while (SherpaOnnxIsOnlineStreamReady(recognizer_, stream_))
            SherpaOnnxDecodeOnlineStream(recognizer_, stream_);
        const SherpaOnnxOnlineRecognizerResult* r = SherpaOnnxGetOnlineStreamResult(recognizer_, stream_);
        tokens->clear();
        if (r != nullptr) {
            for (int32_t i = 0; i < r->count; ++i) {
                std::vector<std::string> symbols;
                if (identity_) {
                    const std::string unit = CollapseRuns(r->tokens_arr[i], collapse_);
                    if (unit.empty() || unit[0] == '<')
                        continue;  // blank and other markers
                    symbols.push_back(unit);
                } else {
                    const auto it = map_.find(r->tokens_arr[i]);
                    if (it == map_.end())
                        continue;
                    symbols = it->second;
                }
                // timestamps are seconds; the model's output step is 40 ms = 4 frames of 10 ms
                const int start = r->timestamps != nullptr
                                      ? static_cast<int>(r->timestamps[i] * 100.0f + 0.5f)
                                      : static_cast<int>(i) * 4;
                int offset = 0;
                for (const std::string& symbol : symbols) {
                    HeardToken t;
                    t.symbol = symbol;
                    t.confidence = 1.0;  // sherpa-onnx gives no per-token confidence
                    t.start = start + offset;
                    t.end = t.start + 4;
                    tokens->push_back(t);
                    offset += 1;
                }
            }
        }
        SherpaOnnxDestroyOnlineRecognizerResult(r);
    }

    const SherpaOnnxOnlineRecognizer* recognizer_;
    TokenMap map_;
    bool identity_;
    int collapse_;
    const SherpaOnnxOnlineStream* stream_;
};

bool ReadFile(const std::string& path, std::string* out) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    *out = ss.str();
    return true;
}

}  // namespace

std::unique_ptr<TokenRecognizer> CreateSherpaRecognizer(const SherpaConfig& config, std::string* error) {
    TokenMap map;
    const bool identity = config.token_map.empty();
    if (!identity) {
        std::string map_text;
        if (!ReadFile(config.token_map, &map_text)) {
            *error = "cannot read " + config.token_map;
            return nullptr;
        }
        if (!ParseTokenMap(map_text, &map, error))
            return nullptr;
    }

    SherpaOnnxOnlineRecognizerConfig c;
    std::memset(&c, 0, sizeof(c));
    c.feat_config.sample_rate = 16000;
    c.feat_config.feature_dim = 80;
    c.model_config.zipformer2_ctc.model = config.model.c_str();
    c.model_config.tokens = config.tokens.c_str();
    c.model_config.num_threads = config.threads;
    c.model_config.provider = "cpu";
    c.decoding_method = "greedy_search";
    c.enable_endpoint = 0;  // one continuous hypothesis for the whole recitation

    const SherpaOnnxOnlineRecognizer* recognizer = SherpaOnnxCreateOnlineRecognizer(&c);
    if (recognizer == nullptr) {
        *error = "sherpa-onnx could not load the model " + config.model;
        return nullptr;
    }
    return std::make_unique<SherpaRecognizer>(recognizer, std::move(map), identity, config.collapse_runs);
}

#endif  // MUTQIN_HAVE_SHERPA

}  // namespace mutqin
