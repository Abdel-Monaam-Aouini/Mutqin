// Interface between the audio front end and a recognition model. A backend (sherpa-onnx, ONNX
// Runtime, or a test fake) turns log-mel frames into CTC posteriors over the 32-token vocabulary
// (blank + the 31 phonemes of docs/PHONEME_INVENTORY.md); it keeps its own streaming state.
#ifndef MUTQIN_INFERENCE_ACOUSTIC_MODEL_H
#define MUTQIN_INFERENCE_ACOUSTIC_MODEL_H

#include <cstddef>
#include <vector>

namespace mutqin {

class AcousticModel {
public:
    virtual ~AcousticModel() = default;

    // Consumes `frames` log-mel frames (80 floats each, 10 ms apart) and appends the posterior rows
    // that became available (kVocabSize floats each, each row summing to about 1) to *posteriors.
    virtual void Accept(const float* feats, size_t frames, std::vector<float>* posteriors) = 0;

    // Forgets all streaming state (start of a new recitation).
    virtual void Reset() = 0;
};

}  // namespace mutqin

#endif  // MUTQIN_INFERENCE_ACOUSTIC_MODEL_H
