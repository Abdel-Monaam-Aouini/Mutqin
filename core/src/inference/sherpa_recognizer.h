// sherpa-onnx (Apache-2.0) backend: a streaming CTC model run through its C API. Compiled only
// with -DMUTQIN_WITH_SHERPA=ON and SHERPA_ONNX_DIR pointing at a sherpa-onnx release.
#ifndef MUTQIN_INFERENCE_SHERPA_RECOGNIZER_H
#define MUTQIN_INFERENCE_SHERPA_RECOGNIZER_H

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "inference/token_recognizer.h"

namespace mutqin {

struct SherpaConfig {
    std::string model;      // streaming Zipformer2 CTC ONNX file
    std::string tokens;     // the model's tokens.txt
    std::string token_map;  // lines "<model unit><TAB><our symbols separated by spaces>"
    int threads = 1;
};

// Our phoneme symbols for each model unit. Units missing from the map are dropped (blank, markers).
using TokenMap = std::map<std::string, std::vector<std::string>>;

// Parses a token map; every symbol must be in the phoneme inventory. Sets *error on failure.
bool ParseTokenMap(const std::string& text, TokenMap* out, std::string* error);

// Reads "key=value" lines (model, tokens, token_map, threads), paths relative to the file's folder.
bool ReadModelManifest(const std::string& path, SherpaConfig* out, std::string* error);

#ifdef MUTQIN_HAVE_SHERPA
std::unique_ptr<TokenRecognizer> CreateSherpaRecognizer(const SherpaConfig& config, std::string* error);
#endif

}  // namespace mutqin

#endif  // MUTQIN_INFERENCE_SHERPA_RECOGNIZER_H
