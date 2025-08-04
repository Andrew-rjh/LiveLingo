#pragma once

#include <string>
#include <vector>

// Transcribe audio using OpenAI's API.
// The audio vector is expected to contain PCM samples at WHISPER_SAMPLE_RATE.
std::string openai_transcribe(const std::vector<float> &audio, const std::string &language);

