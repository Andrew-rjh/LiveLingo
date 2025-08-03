#include "system-audio.h"

#include <algorithm>
#include <cstdio>

system_audio_async::system_audio_async(int len_ms) {
    m_len_ms = len_ms;
    m_running = false;
}

system_audio_async::~system_audio_async() {
    ma_device_uninit(&m_device);
}

bool system_audio_async::init(int /*capture_id*/, int sample_rate) {
    ma_device_config config = ma_device_config_init(ma_device_type_loopback);
    config.capture.format   = ma_format_f32;
    config.capture.channels = 2;
    config.sampleRate       = sample_rate;
    config.dataCallback     = [](ma_device* device, void* /*output*/, const void* input, ma_uint32 frame_count) {
        system_audio_async* audio = static_cast<system_audio_async*>(device->pUserData);
        audio->callback(static_cast<const float*>(input), frame_count);
    };
    config.pUserData = this;

    if (ma_device_init(nullptr, &config, &m_device) != MA_SUCCESS) {
        std::fprintf(stderr, "%s: failed to open loopback device\n", __func__);
        return false;
    }

    m_sample_rate = m_device.sampleRate;
    m_audio.resize((m_sample_rate * m_len_ms) / 1000);
    return true;
}

bool system_audio_async::resume() {
    if (m_running) {
        std::fprintf(stderr, "%s: already running!\n", __func__);
        return false;
    }
    if (ma_device_start(&m_device) != MA_SUCCESS) {
        std::fprintf(stderr, "%s: failed to start device\n", __func__);
        return false;
    }
    m_running = true;
    return true;
}

bool system_audio_async::pause() {
    if (!m_running) {
        std::fprintf(stderr, "%s: not running!\n", __func__);
        return false;
    }
    ma_device_stop(&m_device);
    m_running = false;
    return true;
}

bool system_audio_async::clear() {
    if (!m_running) {
        std::fprintf(stderr, "%s: not running!\n", __func__);
        return false;
    }
    std::lock_guard<std::mutex> lock(m_mutex);
    m_audio_pos = 0;
    m_audio_len = 0;
    return true;
}

void system_audio_async::callback(const float* input, ma_uint32 frame_count) {
    if (!m_running) return;

    std::lock_guard<std::mutex> lock(m_mutex);
    for (ma_uint32 i = 0; i < frame_count; ++i) {
        float sample = 0.5f * (input[2 * i] + input[2 * i + 1]);
        m_audio[m_audio_pos] = sample;
        m_audio_pos = (m_audio_pos + 1) % m_audio.size();
        m_audio_len = std::min(m_audio_len + 1, m_audio.size());
    }
}

void system_audio_async::get(int ms, std::vector<float>& audio) {
    if (!m_running) {
        std::fprintf(stderr, "%s: not running!\n", __func__);
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    if (ms <= 0) {
        ms = m_len_ms;
    }

    size_t n_samples = (m_sample_rate * ms) / 1000;
    if (n_samples > m_audio_len) {
        n_samples = m_audio_len;
    }

    audio.resize(n_samples);
    int s0 = static_cast<int>(m_audio_pos) - static_cast<int>(n_samples);
    if (s0 < 0) {
        s0 += m_audio.size();
    }
    for (size_t i = 0; i < n_samples; ++i) {
        audio[i] = m_audio[(s0 + i) % m_audio.size()];
    }
}

