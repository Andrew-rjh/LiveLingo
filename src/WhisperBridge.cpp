#include "WhisperBridge.h"
#include "whisper.h"
#include <fstream>
#include <iostream>
#include <vector>
#include <cstring>
#include <thread>

namespace WhisperBridge {

// Minimal WAV loader for 16-bit PCM mono. The previous implementation assumed a
// fixed header layout which failed on files containing additional chunks.  This
// parser walks the RIFF chunks to robustly locate the "fmt " and "data" blocks.
static bool load_wav(const std::string &path, std::vector<float> &pcmf32, int &sampleRate) {
    struct ChunkHeader {
        char id[4];
        uint32_t size;
    };

    std::ifstream ifs(path, std::ios::binary);
    if (!ifs) return false;

    char riff[4];
    uint32_t riffSize = 0;
    char wave[4];
    ifs.read(riff, 4);
    ifs.read(reinterpret_cast<char*>(&riffSize), 4);
    ifs.read(wave, 4);
    if (std::strncmp(riff, "RIFF", 4) != 0 || std::strncmp(wave, "WAVE", 4) != 0) {
        return false;
    }

    uint16_t numChannels = 0;
    uint16_t bitsPerSample = 0;
    uint16_t audioFormat = 0;
    uint32_t dataSize = 0;
    std::vector<char> data;
    bool fmtFound = false;

    while (ifs) {
        ChunkHeader ch{};
        if (!ifs.read(reinterpret_cast<char*>(&ch), sizeof(ch))) break;

        if (std::strncmp(ch.id, "fmt ", 4) == 0) {
            std::vector<char> fmtData(ch.size);
            if (!ifs.read(fmtData.data(), ch.size)) return false;
            if (ch.size < 16) return false;
            audioFormat   = *reinterpret_cast<uint16_t*>(&fmtData[0]);
            numChannels   = *reinterpret_cast<uint16_t*>(&fmtData[2]);
            sampleRate    = *reinterpret_cast<uint32_t*>(&fmtData[4]);
            bitsPerSample = *reinterpret_cast<uint16_t*>(&fmtData[14]);
            fmtFound = true;
            // skip any remaining bytes in fmt chunk
            if (ch.size > 16) {
                // position is already at end of chunk because we read ch.size bytes
            }
        } else if (std::strncmp(ch.id, "data", 4) == 0) {
            dataSize = ch.size;
            data.resize(dataSize);
            if (!ifs.read(data.data(), dataSize)) return false;
            break; // stop after the data chunk
        } else {
            // Skip unknown chunk
            ifs.seekg(ch.size, std::ios::cur);
        }
    }

    if (!fmtFound || dataSize == 0) return false;
    if (audioFormat != 1 /*PCM*/ || bitsPerSample != 16 || numChannels == 0) {
        // unsupported format
        return false;
    }

    size_t bytesPerSample = bitsPerSample / 8;
    size_t numFrames = dataSize / (bytesPerSample * numChannels);
    const int16_t* pcm16 = reinterpret_cast<const int16_t*>(data.data());

    pcmf32.resize(numFrames);
    for (size_t i = 0; i < numFrames; ++i) {
        int32_t sum = 0;
        for (uint16_t ch = 0; ch < numChannels; ++ch) {
            sum += pcm16[i * numChannels + ch];
        }
        float sample = static_cast<float>(sum) / (32768.0f * numChannels);
        pcmf32[i] = sample;
    }
    return true;
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
