#include "vad/energy_vad.h"

#include <cmath>

namespace mutqin {

bool EnergyVad::ProcessBlock(const float* samples) {
    double sum = 0.0;
    for (int i = 0; i < config_.block; ++i)
        sum += static_cast<double>(samples[i]) * samples[i];
    const double rms = std::sqrt(sum / config_.block);
    const double db = 20.0 * std::log10(rms > 1e-9 ? rms : 1e-9);
    if (!active_) {
        if (db > config_.start_db) {
            active_ = true;
            quiet_ = 0;
        }
    } else if (db < config_.stop_db) {
        if (++quiet_ > config_.hangover_blocks)
            active_ = false;
    } else {
        quiet_ = 0;
    }
    return active_;
}

}  // namespace mutqin
