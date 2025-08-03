#pragma once

#include <vector>

// Returns true if the given PCM audio contains human speech.
bool vad_detect_speech(const std::vector<float> &pcmf32, int sample_rate);

