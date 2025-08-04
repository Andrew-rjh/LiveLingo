#define _CRT_SECURE_NO_WARNINGS
#include "openai_client.h"
#include "common.h"
#include "whisper.h"

#include <curl/curl.h>
#include <cstdlib>
#include <fstream>
#include <sstream>

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

