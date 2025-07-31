#include "MuLaw.h"
#include <cmath>

unsigned char linearToMuLaw(int16_t sample) {
    const double MU = 255.0;
    double normalized = sample / 32768.0;
    double sign = normalized < 0 ? -1.0 : 1.0;
    normalized = std::abs(normalized);
    double magnitude = std::log(1.0 + MU * normalized) / std::log(1.0 + MU);
    int mu = static_cast<int>(((sign * magnitude) + 1.0) * 127.5);
    if (mu < 0) mu = 0;
    if (mu > 255) mu = 255;
    return static_cast<unsigned char>(mu);
}

int16_t muLawToLinear(unsigned char mu) {
    const double MU = 255.0;
    double normalized = (static_cast<int>(mu) / 255.0) * 2.0 - 1.0;
    double sign = normalized < 0 ? -1.0 : 1.0;
    normalized = std::abs(normalized);
    double magnitude = (std::pow(1.0 + MU, normalized) - 1.0) / MU;
    double sample = sign * magnitude * 32768.0;
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;
    return static_cast<int16_t>(sample);
}
