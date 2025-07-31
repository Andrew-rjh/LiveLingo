#ifndef AUDIO_CAPTURE_H
#define AUDIO_CAPTURE_H

#include <thread>
#include <atomic>
#include <string>
#include "AudioBuffer.h"

#ifdef _WIN32
#include <mmdeviceapi.h>
#include <audioclient.h>
#endif

class AudioCapture {
public:
    AudioCapture(bool loopback);
    ~AudioCapture();

    bool start();
    void stop();

    AudioBuffer& buffer();

    int sampleRate() const { return m_sampleRate; }
    short channels() const { return m_channels; }
    short bitsPerSample() const { return m_bitsPerSample; }
    short formatType() const { return m_formatType; }

private:
    void captureThread();

    bool m_loopback;
    std::thread m_thread;
    std::atomic<bool> m_running{false};

    AudioBuffer m_buffer;

    int m_sampleRate = 0;
    short m_channels = 0;
    short m_bitsPerSample = 0;
    short m_blockAlign = 0;
    short m_formatType = 1;

#ifdef _WIN32
    IMMDevice* m_device = nullptr;
    IAudioClient* m_audioClient = nullptr;
    IAudioCaptureClient* m_captureClient = nullptr;
    HANDLE m_event = nullptr;
#endif
};

#endif // AUDIO_CAPTURE_H
