// Single-producer single-consumer float ring buffer. Fixed capacity, no allocation or locks after
// construction, so Push/Pop are safe on the audio path.
#ifndef MUTQIN_AUDIO_RING_BUFFER_H
#define MUTQIN_AUDIO_RING_BUFFER_H

#include <atomic>
#include <cstddef>
#include <vector>

namespace mutqin {

class RingBuffer {
public:
    explicit RingBuffer(size_t capacity) : data_(capacity + 1) {}

    size_t capacity() const { return data_.size() - 1; }
    size_t size() const {
        const size_t w = write_.load(std::memory_order_acquire), r = read_.load(std::memory_order_acquire);
        return (w + data_.size() - r) % data_.size();
    }

    // Producer: copies as many samples as fit, returns how many were stored.
    size_t Push(const float* samples, size_t count) {
        const size_t room = capacity() - size();
        if (count > room)
            count = room;
        size_t w = write_.load(std::memory_order_relaxed);
        for (size_t i = 0; i < count; ++i) {
            data_[w] = samples[i];
            w = (w + 1) % data_.size();
        }
        write_.store(w, std::memory_order_release);
        return count;
    }

    // Consumer: copies up to `count` samples out, returns how many were read.
    size_t Pop(float* out, size_t count) {
        const size_t avail = size();
        if (count > avail)
            count = avail;
        size_t r = read_.load(std::memory_order_relaxed);
        for (size_t i = 0; i < count; ++i) {
            out[i] = data_[r];
            r = (r + 1) % data_.size();
        }
        read_.store(r, std::memory_order_release);
        return count;
    }

private:
    std::vector<float> data_;
    std::atomic<size_t> write_{0};
    std::atomic<size_t> read_{0};
};

}  // namespace mutqin

#endif  // MUTQIN_AUDIO_RING_BUFFER_H
