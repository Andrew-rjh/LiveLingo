#ifndef WHISPER_BRIDGE_H
#define WHISPER_BRIDGE_H

#include <string>

namespace WhisperBridge {
    // Transcribe the given WAV file using whisper.cpp
    void transcribeFile(const std::string &modelPath, const std::string &wavPath);
}

#endif // WHISPER_BRIDGE_H
