#include "common.h"
#include <cmath>
#include <algorithm>

wav_writer::~wav_writer() {
    if (m_file.is_open()) {
        m_file.close();
    }
}

bool wav_writer::open(const std::string & filename, int sample_rate, int bits_per_sample, int channels) {
    m_file.open(filename, std::ios::binary);
    if (!m_file.is_open()) return false;

    uint32_t byte_rate   = sample_rate * channels * bits_per_sample/8;
    uint32_t block_align = channels * bits_per_sample/8;

    m_file.write("RIFF", 4);
    uint32_t chunk_size = 36;
    m_file.write(reinterpret_cast<char*>(&chunk_size), 4);
    m_file.write("WAVE", 4);
    m_file.write("fmt ", 4);
    uint32_t subchunk1_size = 16;
    m_file.write(reinterpret_cast<char*>(&subchunk1_size), 4);
    uint16_t audio_format = 1;
    m_file.write(reinterpret_cast<char*>(&audio_format), 2);
    uint16_t num_channels = channels;
    m_file.write(reinterpret_cast<char*>(&num_channels), 2);
    uint32_t sample_rate_u = sample_rate;
    m_file.write(reinterpret_cast<char*>(&sample_rate_u), 4);
    m_file.write(reinterpret_cast<char*>(&byte_rate), 4);
    m_file.write(reinterpret_cast<char*>(&block_align), 2);
    uint16_t bps = bits_per_sample;
    m_file.write(reinterpret_cast<char*>(&bps), 2);
    m_file.write("data", 4);
    uint32_t data_chunk_size = 0;
    m_file.write(reinterpret_cast<char*>(&data_chunk_size), 4);
    return true;
}

void wav_writer::write(const float * data, size_t length) {
    if (!m_file.is_open()) return;
    for (size_t i = 0; i < length; ++i) {
        int16_t sample = (int16_t) std::max(-1.0f, std::min(1.0f, data[i])) * 32767;
        m_file.write(reinterpret_cast<const char*>(&sample), sizeof(sample));
    }
}

bool vad_simple(const std::vector<float> & pcmf32, int sample_rate, int last_ms, float vad_thold, float freq_thold, bool verbose) {
    (void) freq_thold;
    (void) verbose;
    if (pcmf32.empty()) return false;
    int n_samples = std::min<int>(pcmf32.size(), last_ms * sample_rate / 1000);
    double energy = 0.0;
    for (int i = pcmf32.size() - n_samples; i < (int) pcmf32.size(); ++i) {
        float s = pcmf32[i];
        energy += s * s;
    }
    energy /= std::max(1, n_samples);
    return energy > vad_thold * vad_thold;
}

