#include "vad.h"
#include <cmath>

bool vad_detect_speech(const std::vector<float> &pcmf32, int sample_rate) {
    if (pcmf32.empty()) {
        return false;
    }

    // Calculate short-term energy and zero-crossing rate
    float energy = 0.0f;
    int crossings = 0;
    for (size_t i = 0; i < pcmf32.size(); ++i) {
        float s = pcmf32[i];
        energy += s * s;
        if (i > 0) {
            float prev = pcmf32[i - 1];
            if ((s >= 0 && prev < 0) || (s < 0 && prev >= 0)) {
                crossings++;
            }
        }
    }
    energy /= pcmf32.size();
    float zcr = static_cast<float>(crossings) / pcmf32.size();

    // Simple heuristics: voice has enough energy and moderate zero crossings
    const float energy_th = 1e-4f; // around -40 dB
    const float zcr_min = 0.005f;
    const float zcr_max = 0.3f;

    if (energy < energy_th) {
        return false;
    }
    if (zcr < zcr_min || zcr > zcr_max) {
        return false;
    }
    return true;
}

