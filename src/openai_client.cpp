#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS

#include <algorithm>
#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <curl/curl.h>
#include <chrono>
#include <thread>

#include "openai_client.h"
#include "common.h"
#include "whisper.h"


// simple helper to read environment variable or fallback to .env file
static std::string get_env(const std::string &key) {
    const char *val = std::getenv(key.c_str());
    if (val) {
        return std::string(val);
    }
    std::ifstream env(".env");
    std::string line;
    while (std::getline(env, line)) {
        if (line.empty() || line[0] == '#') continue;
        size_t pos = line.find('=');
        if (pos == std::string::npos) continue;
        std::string k = line.substr(0, pos);
        std::string v = line.substr(pos + 1);
        if (k == key) {
            return v;
        }
    }
    return "";
}

static size_t write_callback(void *contents, size_t size, size_t nmemb, void *userp) {
    size_t total = size * nmemb;
    static_cast<std::string*>(userp)->append(static_cast<char*>(contents), total);
    return total;
}

// basic base64 encoder
static std::string base64_encode(const unsigned char *data, size_t len) {
    static const char table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((len + 2) / 3) * 4);
    int val = 0;
    int valb = -6;
    for (size_t i = 0; i < len; ++i) {
        unsigned char c = data[i];
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            out.push_back(table[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    if (valb > -6) out.push_back(table[((val << 8) >> (valb + 8)) & 0x3F]);
    while (out.size() % 4) out.push_back('=');
    return out;
}

static std::string pcmf32_to_base64(const std::vector<float> &audio) {
    std::vector<int16_t> pcm16(audio.size());
    for (size_t i = 0; i < audio.size(); ++i) {
        float v = std::max(-1.0f, std::min(1.0f, audio[i]));
        pcm16[i] = (int16_t) (v * 32767.0f);
    }
    const unsigned char *bytes = reinterpret_cast<const unsigned char*>(pcm16.data());
    return base64_encode(bytes, pcm16.size() * sizeof(int16_t));
}

std::string openai_transcribe(const std::vector<float> &audio, const std::string &language) {
    std::string api_key = get_env("OPENAI_API_KEY");
    if (api_key.empty()) {
        fprintf(stderr, "OPENAI_API_KEY is not set\n");
        return "";
    }

    // Save audio to temporary wav file
    const std::string tmp_file = "openai_tmp.wav";
    wav_writer writer;
    writer.open(tmp_file, WHISPER_SAMPLE_RATE, 16, 1);
    writer.write(audio.data(), audio.size());
    writer.close();

    CURL *curl = curl_easy_init();
    if (!curl) {
        return "";
    }

    struct curl_httppost *form = nullptr;
    struct curl_httppost *last = nullptr;

    curl_formadd(&form, &last,
                 CURLFORM_COPYNAME, "model",
                 CURLFORM_COPYCONTENTS, "gpt-4o-mini-transcribe",
                 CURLFORM_END);

    curl_formadd(&form, &last,
                 CURLFORM_COPYNAME, "file",
                 CURLFORM_FILE, tmp_file.c_str(),
                 CURLFORM_END);

    curl_formadd(&form, &last,
                 CURLFORM_COPYNAME, "response_format",
                 CURLFORM_COPYCONTENTS, "text",
                 CURLFORM_END);

    curl_formadd(&form, &last,
                 CURLFORM_COPYNAME, "language",
                 CURLFORM_COPYCONTENTS, language.c_str(),
                 CURLFORM_END);

    std::string response;
    struct curl_slist *headers = nullptr;
    std::string auth = "Authorization: Bearer " + api_key;
    headers = curl_slist_append(headers, auth.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, "https://api.openai.com/v1/audio/transcriptions");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_HTTPPOST, form);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);

    curl_easy_cleanup(curl);
    curl_formfree(form);
    curl_slist_free_all(headers);

    if (res != CURLE_OK) {
        fprintf(stderr, "OpenAI request failed: %s\n", curl_easy_strerror(res));
        return "";
    }

    return response;
}

OpenAIRealtimeClient::OpenAIRealtimeClient(const std::string &language)
    : m_language(language) {}

OpenAIRealtimeClient::~OpenAIRealtimeClient() {
    if (m_curl) {
        curl_easy_cleanup(m_curl);
    }
    if (m_headers) {
        curl_slist_free_all(m_headers);
    }
}

bool OpenAIRealtimeClient::connect() {
    std::string api_key = get_env("OPENAI_API_KEY");
    if (api_key.empty()) {
        fprintf(stderr, "OPENAI_API_KEY is not set\n");
        return false;
    }

    m_curl = curl_easy_init();
    if (!m_curl) {
        return false;
    }

    std::string url = "wss://api.openai.com/v1/realtime?intent=transcription&model=gpt-4o-mini-transcribe&version=2025-04-01-preview";
    curl_easy_setopt(m_curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(m_curl, CURLOPT_CONNECT_ONLY, 2L); // 2 = websockets
    curl_easy_setopt(m_curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);

    std::string auth = "Authorization: Bearer " + api_key;
    m_headers = curl_slist_append(m_headers, auth.c_str());
    // specify OpenAI realtime subprotocol so the server accepts the websocket handshake
    m_headers = curl_slist_append(m_headers, "Sec-WebSocket-Protocol: oai.realtime.v1");
    curl_easy_setopt(m_curl, CURLOPT_HTTPHEADER, m_headers);

    CURLcode res = curl_easy_perform(m_curl);
    if (res != CURLE_OK) {
        fprintf(stderr, "OpenAI WS connect failed: %s\n", curl_easy_strerror(res));
        return false;
    }

    // send config message
    std::string cfg = "{\"type\":\"config\",\"model\":\"gpt-4o-mini-transcribe\",\"language\":\"" + m_language + "\"}";
    size_t sent = 0;
    res = curl_ws_send(m_curl, cfg.c_str(), cfg.size(), &sent, 0, CURLWS_TEXT);
    if (res != CURLE_OK) {
        fprintf(stderr, "OpenAI WS config failed: %s\n", curl_easy_strerror(res));
        return false;
    }
    return true;
}

bool OpenAIRealtimeClient::send_audio(const std::vector<float> &audio) {
    if (!m_curl) return false;
    std::string b64 = pcmf32_to_base64(audio);
    std::string msg = "{\"type\":\"audio_data\",\"data\":\"" + b64 + "\"}";
    size_t sent = 0;
    CURLcode res = curl_ws_send(m_curl, msg.c_str(), msg.size(), &sent, 0, CURLWS_TEXT);
    return res == CURLE_OK;
}

bool OpenAIRealtimeClient::receive_transcript(std::string &text) {
    if (!m_curl) return false;
    char buf[4096];
    size_t nread = 0;
    const struct curl_ws_frame *meta = nullptr;
    CURLcode res = curl_ws_recv(m_curl, buf, sizeof(buf), &nread, &meta);
    if (res == CURLE_AGAIN || nread == 0) {
        return false;
    }
    if (res != CURLE_OK) {
        fprintf(stderr, "OpenAI WS recv failed: %s\n", curl_easy_strerror(res));
        return false;
    }

    std::string resp(buf, nread);
    auto pos = resp.find("\"text\":");
    if (pos == std::string::npos) return false;
    pos = resp.find('"', pos + 7);
    if (pos == std::string::npos) return false;
    size_t end = resp.find('"', pos + 1);
    if (end == std::string::npos) return false;
    text = resp.substr(pos + 1, end - pos - 1);
    return true;
}

bool OpenAIRealtimeClient::ping() {
    if (!m_curl) return false;
    size_t sent = 0;
    CURLcode res = curl_ws_send(m_curl, "", 0, &sent, 0, CURLWS_PING);
    return res == CURLE_OK;
}

