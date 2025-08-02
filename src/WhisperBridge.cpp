#include "WhisperBridge.h"
#include "whisper.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <thread>
#include <cmath>
#include <algorithm>

namespace WhisperBridge {

// Simple WAV loader that supports 16-bit PCM or 32-bit float with arbitrary channel
// count.  Multi-channel audio is downmixed to mono by averaging the channels and
// resampled to WHISPER_SAMPLE_RATE if needed.
static bool load_wav(const std::string &path, std::vector<float> &pcmf32, int &sampleRate) {
    struct WavHeader {
        char riff[4];
        uint32_t chunkSize;
        char wave[4];
        char fmt[4];
        uint32_t subchunk1Size;
        uint16_t audioFormat;
        uint16_t numChannels;
        uint32_t sampleRate;
        uint32_t byteRate;
        uint16_t blockAlign;
        uint16_t bitsPerSample;
        char data[4];
        uint32_t dataSize;
    } header;

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;
    ifs.read(reinterpret_cast<char*>(&header), sizeof(header));
    if (std::strncmp(header.riff, "RIFF", 4) != 0 || std::strncmp(header.wave, "WAVE", 4) != 0) {
        return false;
    }
    // We support 16-bit PCM and 32-bit float.
    if (!(header.audioFormat == 1 || header.audioFormat == 3)) {
        return false;
    }

    sampleRate = header.sampleRate;

    const size_t bytesPerSample = header.bitsPerSample / 8;
    const size_t frameCount = header.dataSize / (bytesPerSample * header.numChannels);

    std::vector<float> tmp(frameCount * header.numChannels);
    if (header.audioFormat == 1 && header.bitsPerSample == 16) {
        std::vector<int16_t> pcm16(frameCount * header.numChannels);
        ifs.read(reinterpret_cast<char*>(pcm16.data()), header.dataSize);
        for (size_t i = 0; i < pcm16.size(); ++i) {
            tmp[i] = static_cast<float>(pcm16[i]) / 32768.0f;
        }
    } else if (header.bitsPerSample == 32) {
        ifs.read(reinterpret_cast<char*>(tmp.data()), header.dataSize);
    } else {
        return false;
    }

    // Downmix to mono by averaging channels
    pcmf32.resize(frameCount);
    for (size_t i = 0; i < frameCount; ++i) {
        float sum = 0.0f;
        for (int c = 0; c < header.numChannels; ++c) {
            sum += tmp[i * header.numChannels + c];
        }
        pcmf32[i] = sum / header.numChannels;
    }

    // Resample if needed using simple linear interpolation
    if (sampleRate != WHISPER_SAMPLE_RATE && sampleRate > 0) {
        const size_t newFrameCount =
            static_cast<size_t>((static_cast<uint64_t>(frameCount) * WHISPER_SAMPLE_RATE) / sampleRate);
        std::vector<float> resampled(newFrameCount);
        for (size_t i = 0; i < newFrameCount; ++i) {
            float pos = static_cast<float>(i) * sampleRate / WHISPER_SAMPLE_RATE;
            size_t idx = static_cast<size_t>(pos);
            float frac = pos - idx;
            float v1 = idx < frameCount ? pcmf32[idx] : 0.0f;
            float v2 = (idx + 1 < frameCount) ? pcmf32[idx + 1] : v1;
            resampled[i] = v1 + (v2 - v1) * frac;
        }
        pcmf32.swap(resampled);
        sampleRate = WHISPER_SAMPLE_RATE;
    }

    return true;
}

// Convert arbitrary PCM data to mono float32 and resample to
// WHISPER_SAMPLE_RATE. Supports 16-bit PCM and 32-bit float input.
static bool convert_pcm(const std::vector<char> &pcm,
                        int sampleRate,
                        int channels,
                        int bitsPerSample,
                        int formatType,
                        std::vector<float> &pcmf32) {
    const size_t bytesPerSample = bitsPerSample / 8;
    if (bytesPerSample == 0 || channels <= 0) return false;
    const size_t frameCount = pcm.size() / (bytesPerSample * channels);
    std::vector<float> tmp(frameCount * channels);
    if (formatType == 1 && bitsPerSample == 16) {
        const int16_t* data = reinterpret_cast<const int16_t*>(pcm.data());
        for (size_t i = 0; i < frameCount * channels; ++i) {
            tmp[i] = static_cast<float>(data[i]) / 32768.0f;
        }
    } else if (formatType == 3 && bitsPerSample == 32) {
        const float* data = reinterpret_cast<const float*>(pcm.data());
        std::copy(data, data + frameCount * channels, tmp.begin());
    } else {
        return false;
    }

    pcmf32.resize(frameCount);
    for (size_t i = 0; i < frameCount; ++i) {
        float sum = 0.0f;
        for (int c = 0; c < channels; ++c) {
            sum += tmp[i * channels + c];
        }
        pcmf32[i] = sum / channels;
    }

    if (sampleRate != WHISPER_SAMPLE_RATE && sampleRate > 0) {
        const size_t newFrameCount =
            static_cast<size_t>((static_cast<uint64_t>(frameCount) * WHISPER_SAMPLE_RATE) / sampleRate);
        std::vector<float> resampled(newFrameCount);
        for (size_t i = 0; i < newFrameCount; ++i) {
            float pos = static_cast<float>(i) * sampleRate / WHISPER_SAMPLE_RATE;
            size_t idx = static_cast<size_t>(pos);
            float frac = pos - idx;
            float v1 = idx < frameCount ? pcmf32[idx] : 0.0f;
            float v2 = (idx + 1 < frameCount) ? pcmf32[idx + 1] : v1;
            resampled[i] = v1 + (v2 - v1) * frac;
        }
        pcmf32.swap(resampled);
    }
    return true;
}

void transcribeFile(const std::string &modelPath,
                    const std::string &wavPath,
                    const std::string &language) {
    std::vector<float> pcmf32;
    int sampleRate = 0;
    if (!load_wav(wavPath, pcmf32, sampleRate)) {
        std::cerr << "Failed to load WAV file: " << wavPath << std::endl;
        return;
    }
    if (sampleRate != WHISPER_SAMPLE_RATE) {
        std::cerr << "Unsupported sample rate" << std::endl;
        return;
    }

    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = true;

    struct whisper_context* ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);

    if (!ctx) {
        std::cerr << "Failed to load model: " << modelPath << std::endl;
        return;
    }

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress   = false;
    params.print_realtime   = false;
    params.print_timestamps = false;
    if (!language.empty()) {
        params.language = language.c_str();
        params.detect_language = false;
    } else {
        params.language = "";
        params.detect_language = true;
    }
    params.n_threads        = std::max(1u, std::thread::hardware_concurrency());

    if (whisper_full(ctx, params, pcmf32.data(), pcmf32.size()) != 0) {
        std::cerr << "whisper_full() failed" << std::endl;
        whisper_free(ctx);
        return;
    }

    int n = whisper_full_n_segments(ctx);
    for (int i = 0; i < n; ++i) {
        const char * text = whisper_full_get_segment_text(ctx, i);
        std::cout << text << std::endl;
    }

    whisper_free(ctx);
}

WhisperStream::WhisperStream(const std::string &modelPath,
                             const std::string &language)
    : m_language(language) {
    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = true;
    m_ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);
    if (!m_ctx) {
        std::cerr << "Failed to load model: " << modelPath << std::endl;
    }
}

WhisperStream::~WhisperStream() {
    if (m_ctx) {
        whisper_free(m_ctx);
    }
}

std::string WhisperStream::transcribe(const std::vector<char> &pcm,
                                      int sampleRate,
                                      int channels,
                                      int bitsPerSample,
                                      int formatType) {
    if (!m_ctx) return {};
    std::vector<float> pcmf32;
    if (!convert_pcm(pcm, sampleRate, channels, bitsPerSample, formatType, pcmf32)) {
        return {};
    }
    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress   = false;
    params.print_realtime   = false;
    params.print_timestamps = false;
    if (!m_language.empty()) {
        params.language = m_language.c_str();
        params.detect_language = false;
    } else {
        params.language = "";
        params.detect_language = true;
    }
    params.n_threads        = std::max(1u, std::thread::hardware_concurrency());

    if (whisper_full(m_ctx, params, pcmf32.data(), pcmf32.size()) != 0) {
        std::cerr << "whisper_full() failed" << std::endl;
        return {};
    }

    std::string text;
    int n = whisper_full_n_segments(m_ctx);
    for (int i = 0; i < n; ++i) {
        const char * seg = whisper_full_get_segment_text(m_ctx, i);
        text += seg;
    }
    whisper_reset_state(m_ctx);
    return text;
}

} // namespace WhisperBridge
