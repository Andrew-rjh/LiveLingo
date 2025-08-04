#pragma once

#include <string>
#include <vector>
#include <curl/curl.h>

// Transcribe audio using OpenAI's API.
// The audio vector is expected to contain PCM samples at WHISPER_SAMPLE_RATE.
std::string openai_transcribe(const std::vector<float> &audio, const std::string &language);

// Simple WebSocket client for the OpenAI realtime transcription API
class OpenAIRealtimeClient {
public:
    explicit OpenAIRealtimeClient(const std::string &language);
    ~OpenAIRealtimeClient();

    // establish websocket connection and send the initial config message
    bool connect();

    // send a chunk of PCM audio (float samples in [-1,1])
    bool send_audio(const std::vector<float> &audio);

    // receive a transcript message, returns false if no message available
    bool receive_transcript(std::string &text);

private:
    std::string m_language;
    CURL *m_curl = nullptr;
    struct curl_slist *m_headers = nullptr;
};

