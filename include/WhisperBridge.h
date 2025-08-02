#ifndef WHISPER_BRIDGE_H
#define WHISPER_BRIDGE_H

#include <string>
#include <vector>

namespace WhisperBridge {
    // Initialize the Whisper model once and reuse for subsequent
    // transcriptions. Returns true on success.
    bool init(const std::string &modelPath);

    // Transcribe raw PCM audio samples. The audio data should contain
    // interleaved samples in little-endian format. Supported bit depths
    // are 16-bit integer and 32-bit float. The buffer is down-mixed to
    // mono and resampled to the model's expected sample rate internally.
    std::string transcribeBuffer(const std::vector<char> &audioData,
                                 int sampleRate,
                                 int numChannels,
                                 int bitsPerSample,
                                 const std::string &language = "");

    // Free the model resources when no longer needed.
    void cleanup();
}

#endif // WHISPER_BRIDGE_H
