#ifndef WHISPER_BRIDGE_H
#define WHISPER_BRIDGE_H

#include <string>

namespace WhisperBridge {
    // Transcribe the given WAV file using whisper.cpp. Language can be
    // specified (e.g. "en" or "ko"). If empty, whisper.cpp will try to
    // detect the language automatically.
    void transcribeFile(const std::string &modelPath,
                        const std::string &wavPath,
                        const std::string &language = "");
}

#endif // WHISPER_BRIDGE_H
