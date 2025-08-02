#pragma once

#include "ggml-backend.h"
#include <string>
#include <map>
#include <vector>
#include <random>
#include <thread>
#include <ctime>
#include <fstream>
#include <sstream>

// simplified copy from whisper.cpp examples

class wav_writer {
public:
    wav_writer() = default;
    ~wav_writer();

    bool open(const std::string & filename, int sample_rate, int bits_per_sample, int channels);
    void write(const float * data, size_t length);

private:
    std::ofstream m_file;
};

bool vad_simple(
        const std::vector<float> & pcmf32,
        int   sample_rate,
        int   last_ms,
        float vad_thold,
        float freq_thold,
        bool  verbose);

