#include "WhisperBridge.h"
#include "whisper.h"
#include <cstring>
#include <iostream>
#include <thread>

namespace WhisperBridge {

static struct whisper_context* g_ctx = nullptr;

// Convert raw PCM samples to mono float32 PCM at WHISPER_SAMPLE_RATE
static bool convertAudio(const std::vector<char> &audioData,
                         int sampleRate,
                         int numChannels,
                         int bitsPerSample,
                         std::vector<float> &out) {
    if (audioData.empty() || sampleRate <= 0 || numChannels <= 0) {
        return false;
    }

    size_t frameCount = audioData.size() / ((bitsPerSample / 8) * numChannels);
    std::vector<float> tmp(frameCount * numChannels);

    if (bitsPerSample == 16) {
        const int16_t *pcm16 = reinterpret_cast<const int16_t*>(audioData.data());
        for (size_t i = 0; i < frameCount * numChannels; ++i) {
            tmp[i] = static_cast<float>(pcm16[i]) / 32768.0f;
        }
    } else if (bitsPerSample == 24) {
        const unsigned char *pcm24 = reinterpret_cast<const unsigned char*>(audioData.data());
        for (size_t i = 0; i < frameCount * numChannels; ++i) {
            int32_t sample = pcm24[i * 3] | (pcm24[i * 3 + 1] << 8) | (pcm24[i * 3 + 2] << 16);
            if (sample & 0x800000) sample |= ~0xFFFFFF; // sign extend
            tmp[i] = static_cast<float>(sample) / 8388608.0f;
        }
    } else if (bitsPerSample == 32) {
        const float *pcm32 = reinterpret_cast<const float*>(audioData.data());
        std::memcpy(tmp.data(), pcm32, frameCount * numChannels * sizeof(float));
    } else {
        return false; // unsupported format
    }

    // Downmix to mono
    out.resize(frameCount);
    for (size_t i = 0; i < frameCount; ++i) {
        float sum = 0.0f;
        for (int c = 0; c < numChannels; ++c) {
            sum += tmp[i * numChannels + c];
        }
        out[i] = sum / numChannels;
    }

    // Resample if needed using simple linear interpolation
    if (sampleRate != WHISPER_SAMPLE_RATE) {
        const size_t newFrameCount =
            static_cast<size_t>((static_cast<uint64_t>(frameCount) * WHISPER_SAMPLE_RATE) / sampleRate);
        std::vector<float> resampled(newFrameCount);
        for (size_t i = 0; i < newFrameCount; ++i) {
            float pos = static_cast<float>(i) * sampleRate / WHISPER_SAMPLE_RATE;
            size_t idx = static_cast<size_t>(pos);
            float frac = pos - idx;
            float v1 = idx < frameCount ? out[idx] : 0.0f;
            float v2 = (idx + 1 < frameCount) ? out[idx + 1] : v1;
            resampled[i] = v1 + (v2 - v1) * frac;
        }
        out.swap(resampled);
    }

    return true;
}

bool init(const std::string &modelPath) {
    if (g_ctx) {
        return true; // already initialized
    }
    whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = true;
    g_ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);
    if (!g_ctx) {
        std::cerr << "Failed to load model: " << modelPath << std::endl;
        return false;
    }
    return true;
}

std::string transcribeBuffer(const std::vector<char> &audioData,
                             int sampleRate,
                             int numChannels,
                             int bitsPerSample,
                             const std::string &language) {
    if (!g_ctx || audioData.empty()) {
        return ""; // model not initialized or no data
    }

    std::vector<float> pcmf32;
    if (!convertAudio(audioData, sampleRate, numChannels, bitsPerSample, pcmf32)) {
        std::cerr << "Unsupported audio format" << std::endl;
        return "";
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

    if (whisper_full(g_ctx, params, pcmf32.data(), pcmf32.size()) != 0) {
        std::cerr << "whisper_full() failed" << std::endl;
        return "";
    }

    std::string result;
    int n = whisper_full_n_segments(g_ctx);
    for (int i = 0; i < n; ++i) {
        const char *text = whisper_full_get_segment_text(g_ctx, i);
        result += text;
    }
    return result;
}

void cleanup() {
    if (g_ctx) {
        whisper_free(g_ctx);
        g_ctx = nullptr;
    }
}

} // namespace WhisperBridge

