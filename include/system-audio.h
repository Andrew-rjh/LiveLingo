#pragma once

#include "audio-capture.h"
#include "miniaudio.h"

#include <atomic>
#include <mutex>
#include <vector>

//
// System audio capture using miniaudio loopback
//
class system_audio_async : public audio_capture {
public:
    explicit system_audio_async(int len_ms);
    ~system_audio_async();

    bool init(int capture_id, int sample_rate) override;
    bool resume() override;
    bool pause() override;
    bool clear() override;
    void get(int ms, std::vector<float>& audio) override;

    void callback(const float* input, ma_uint32 frame_count);

private:
    ma_device m_device{};
    int m_len_ms = 0;
    int m_sample_rate = 0;
    std::atomic_bool m_running;
    std::mutex m_mutex;
    std::vector<float> m_audio;
    size_t m_audio_pos = 0;
    size_t m_audio_len = 0;
};

