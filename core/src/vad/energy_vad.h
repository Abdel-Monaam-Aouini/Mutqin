// Energy voice activity detector with hangover. Deliberately simple and dependency free; a neural
// VAD (Silero) can replace it behind the same interface once the model stack is chosen.
#ifndef MUTQIN_VAD_ENERGY_VAD_H
#define MUTQIN_VAD_ENERGY_VAD_H

#include <cstddef>

namespace mutqin {

struct EnergyVadConfig {
    int sample_rate = 16000;
    int block = 160;           // samples per decision (10 ms)
    float start_db = -45.0f;   // speech starts above this level (dBFS, RMS)
    float stop_db = -50.0f;    // speech ends below this level
    int hangover_blocks = 30;  // keep "speech" this long after the level drops (300 ms)
};

class EnergyVad {
public:
    explicit EnergyVad(const EnergyVadConfig& config = EnergyVadConfig()) : config_(config) {}

    // Feeds one block of config.block samples; returns true while speech is active.
    bool ProcessBlock(const float* samples);
    bool active() const { return active_; }
    void Reset() {
        active_ = false;
        quiet_ = 0;
    }

private:
    EnergyVadConfig config_;
    bool active_ = false;
    int quiet_ = 0;
};

}  // namespace mutqin

#endif  // MUTQIN_VAD_ENERGY_VAD_H
