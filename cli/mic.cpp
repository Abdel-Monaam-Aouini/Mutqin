#define MINIAUDIO_IMPLEMENTATION
#define MA_NO_DECODING
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_ENGINE
#define MA_NO_NODE_GRAPH
#define MA_NO_RESOURCE_MANAGER
#if defined(_MSC_VER)
#pragma warning(push, 0)  // third-party header
#endif
#include "miniaudio.h"
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <mutex>

#include "mic.h"

namespace mutqin_cli {

struct Microphone::Impl {
    ma_device device;
    bool started = false;
    std::mutex mutex;
    std::vector<float> samples;
};

namespace {
void OnData(ma_device* device, void*, const void* input, ma_uint32 frames) {
    auto* impl = static_cast<Microphone::Impl*>(device->pUserData);
    const float* in = static_cast<const float*>(input);
    std::lock_guard<std::mutex> lock(impl->mutex);  // CLI only: the core's push path is separate
    impl->samples.insert(impl->samples.end(), in, in + frames);
}
}  // namespace

Microphone::Microphone() : impl_(new Impl()) {}

Microphone::~Microphone() {
    if (impl_->started)
        ma_device_uninit(&impl_->device);
}

bool Microphone::Start(std::string* error) {
    ma_device_config config = ma_device_config_init(ma_device_type_capture);
    config.capture.format = ma_format_f32;
    config.capture.channels = 1;
    config.sampleRate = 16000;
    config.dataCallback = OnData;
    config.pUserData = impl_.get();
    if (ma_device_init(nullptr, &config, &impl_->device) != MA_SUCCESS) {
        *error = "cannot open the default microphone";
        return false;
    }
    if (ma_device_start(&impl_->device) != MA_SUCCESS) {
        ma_device_uninit(&impl_->device);
        *error = "cannot start the microphone";
        return false;
    }
    impl_->started = true;
    return true;
}

size_t Microphone::Drain(std::vector<float>* out) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const size_t n = impl_->samples.size();
    out->insert(out->end(), impl_->samples.begin(), impl_->samples.end());
    impl_->samples.clear();
    return n;
}

}  // namespace mutqin_cli
