// A recognizer that works on raw audio and returns recognised phoneme tokens (already mapped to
// our inventory). This is the shape of engines like sherpa-onnx, which decode internally and give
// tokens with timestamps instead of frame posteriors. Confidences are not available from them, so
// every token reports confidence 1.0 and the aligner's low-confidence leniency does not apply.
#ifndef MUTQIN_INFERENCE_TOKEN_RECOGNIZER_H
#define MUTQIN_INFERENCE_TOKEN_RECOGNIZER_H

#include <cstddef>
#include <vector>

#include "align/aligner.h"

namespace mutqin {

class TokenRecognizer {
public:
    virtual ~TokenRecognizer() = default;

    // Feeds 16 kHz mono samples. *tokens is replaced by the COMPLETE hypothesis heard so far
    // (start / end in 10 ms frames), which may still change at its end.
    virtual void Accept(const float* samples, size_t count, std::vector<HeardToken>* tokens) = 0;

    // Signals the end of the audio so the last frames are decoded.
    virtual void Finish(std::vector<HeardToken>* tokens) = 0;

    // Forgets all streaming state (start of a new recitation).
    virtual void Reset() = 0;
};

}  // namespace mutqin

#endif  // MUTQIN_INFERENCE_TOKEN_RECOGNIZER_H
