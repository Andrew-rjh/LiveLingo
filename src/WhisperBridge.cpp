#include "WhisperBridge.h"
#include "whisper.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <thread>

namespace WhisperBridge {

// WAV loader that supports 16-bit PCM and 32-bit float mono
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
    sampleRate = header.sampleRate;
    size_t numSamples = header.dataSize / (header.bitsPerSample / 8);
    pcmf32.resize(numSamples);
    if (header.bitsPerSample == 16) {
        std::vector<int16_t> pcm16(numSamples);
        ifs.read(reinterpret_cast<char*>(pcm16.data()), header.dataSize);
        for (size_t i = 0; i < numSamples; ++i) {
            pcmf32[i] = pcm16[i] / 32768.0f;
        }
        return true;
    } else if (header.bitsPerSample == 32 && header.audioFormat == 3) { // IEEE float
        ifs.read(reinterpret_cast<char*>(pcmf32.data()), header.dataSize);
        return true;
    } else if (header.bitsPerSample == 32 && header.audioFormat == 1) { // 32-bit PCM
        std::vector<int32_t> pcm32(numSamples);
        ifs.read(reinterpret_cast<char*>(pcm32.data()), header.dataSize);
        for (size_t i = 0; i < numSamples; ++i) {
            pcmf32[i] = pcm32[i] / 2147483648.f;
        }
        return true;
    } else {
        std::cerr << "Unsupported WAV format: "
                  << header.audioFormat << " bits:" << header.bitsPerSample << std::endl;
        return false;
    }
}

void transcribeFile(const std::string &modelPath, const std::string &wavPath) {
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
    struct whisper_context* ctx = whisper_init_from_file_with_params(modelPath.c_str(), cparams);

    if (!ctx) {
        std::cerr << "Failed to load model: " << modelPath << std::endl;
        return;
    }

    whisper_full_params params = whisper_full_default_params(WHISPER_SAMPLING_GREEDY);
    params.print_progress   = false;
    params.print_realtime   = false;
    params.print_timestamps = false;
    params.language         = "ko";
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

} // namespace WhisperBridge
