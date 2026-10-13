// Live microphone capture for the CLI (miniaudio). 16 kHz mono float32, as the core expects.
#ifndef MUTQIN_CLI_MIC_H
#define MUTQIN_CLI_MIC_H

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace mutqin_cli {

class Microphone {
public:
    Microphone();
    ~Microphone();
    Microphone(const Microphone&) = delete;
    Microphone& operator=(const Microphone&) = delete;

    // Opens the default capture device and starts it. On failure returns false and sets *error.
    bool Start(std::string* error);
    // Moves the samples captured since the last call into *out (appends); returns how many.
    size_t Drain(std::vector<float>* out);

    struct Impl;  // public only so the capture callback can name it

private:
    std::unique_ptr<Impl> impl_;
};

}  // namespace mutqin_cli

#endif  // MUTQIN_CLI_MIC_H
