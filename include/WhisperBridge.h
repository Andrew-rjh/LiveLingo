#ifndef WHISPER_BRIDGE_H
#define WHISPER_BRIDGE_H

#include <string>
#include <vector>

namespace WhisperBridge {
    // Transcribe the given WAV file using whisper.cpp. Language can be
    // specified (e.g. "en" or "ko"). If empty, whisper.cpp will try to
    // detect the language automatically.
    void transcribeFile(const std::string &modelPath,
                        const std::string &wavPath,
                        const std::string &language = "");

    // Helper class that keeps a whisper.cpp context alive and allows
    // repeatedly transcribing raw PCM data without reloading the model.
    class WhisperStream {
    public:
        WhisperStream(const std::string &modelPath,
                      const std::string &language = "");
        ~WhisperStream();

        // Transcribe raw PCM audio. The buffer can contain 16-bit PCM or
        // 32-bit float samples with an arbitrary channel count. The audio is
        // converted to mono and resampled to WHISPER_SAMPLE_RATE internally.
        std::string transcribe(const std::vector<char> &pcm,
                               int sampleRate,
                               int channels,
                               int bitsPerSample,
                               int formatType);

    private:
        struct whisper_context* m_ctx = nullptr;
        std::string m_language;
    };
}

#endif // WHISPER_BRIDGE_H
