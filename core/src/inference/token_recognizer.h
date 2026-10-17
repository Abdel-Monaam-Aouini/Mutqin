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

    // Feeds 16 kHz mono samples. *tokens is replaced by the hypothesis from token number *from onward
    // (start / end in 10 ms frames); the tokens before *from are final and were returned by earlier calls.
    // The returned tail may still change at its end. Handing over only the tail keeps the cost of a call
    // independent of the length of the recitation.
    virtual void Accept(const float* samples, size_t count, std::vector<HeardToken>* tokens,
                        size_t* from) = 0;

    // Signals the end of the audio so the last frames are decoded (same result convention as Accept).
    virtual void Finish(std::vector<HeardToken>* tokens, size_t* from) = 0;

    // Forgets all streaming state (start of a new recitation).
    virtual void Reset() = 0;
};

}  // namespace mutqin

#endif  // MUTQIN_INFERENCE_TOKEN_RECOGNIZER_H
