#include "AudioCapture.h"
#include <chrono>

#ifdef _WIN32
#include <Functiondiscoverykeys_devpkey.h>
#include <mmdeviceapi.h>
#include <Audioclient.h>
#include <mmreg.h>
#include <comdef.h>
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "winmm.lib")
#endif

AudioCapture::AudioCapture(bool loopback)
    : m_loopback(loopback) {
}

AudioCapture::~AudioCapture() {
    stop();
}

bool AudioCapture::start() {
    if (m_running.load()) return true;
#ifdef _WIN32
    CoInitialize(nullptr);
    IMMDeviceEnumerator* enumerator = nullptr;
    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
                                  IID_PPV_ARGS(&enumerator));
    if (FAILED(hr)) return false;
    hr = enumerator->GetDefaultAudioEndpoint(m_loopback ? eRender : eCapture, eConsole, &m_device);
    enumerator->Release();
    if (FAILED(hr)) return false;
    hr = m_device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&m_audioClient);
    if (FAILED(hr)) return false;
    WAVEFORMATEX* pwfx = nullptr;
    hr = m_audioClient->GetMixFormat(&pwfx);
    if (FAILED(hr)) return false;

    m_sampleRate = pwfx->nSamplesPerSec;
    m_channels = pwfx->nChannels;
    m_bitsPerSample = pwfx->wBitsPerSample;
    m_blockAlign = pwfx->nBlockAlign;
    m_formatType = pwfx->wFormatTag;
#ifdef _WIN32
    if (pwfx->wFormatTag == WAVE_FORMAT_EXTENSIBLE) {
        WAVEFORMATEXTENSIBLE* ext = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(pwfx);
        if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT) {
            m_formatType = WAVE_FORMAT_IEEE_FLOAT;
        } else if (ext->SubFormat == KSDATAFORMAT_SUBTYPE_PCM) {
            m_formatType = WAVE_FORMAT_PCM;
        }
    }
#endif

    size_t bytesPerSecond = static_cast<size_t>(m_sampleRate) * m_blockAlign;
    // Prepare a large enough buffer to hold up to 10 hours of audio.
    m_buffer.setCapacity(bytesPerSecond * 3600 * 10);
    DWORD streamFlags = m_loopback ? AUDCLNT_STREAMFLAGS_LOOPBACK : 0;
    streamFlags |= AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
    hr = m_audioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
                                   streamFlags,
                                   0, 0, pwfx, nullptr);
    if (FAILED(hr)) { CoTaskMemFree(pwfx); return false; }
    hr = m_audioClient->GetService(IID_PPV_ARGS(&m_captureClient));
    if (FAILED(hr)) { CoTaskMemFree(pwfx); return false; }
    m_event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    hr = m_audioClient->SetEventHandle(m_event);
    if (FAILED(hr)) { CoTaskMemFree(pwfx); return false; }
    m_audioClient->Start();
    CoTaskMemFree(pwfx);
    m_running = true;
    m_thread = std::thread(&AudioCapture::captureThread, this);
    return true;
#else
    return false; // unsupported
#endif
}

void AudioCapture::stop() {
    if (!m_running.exchange(false)) return;
#ifdef _WIN32
    if (m_thread.joinable()) m_thread.join();
    if (m_audioClient) {
        m_audioClient->Stop();
    }
    if (m_event) CloseHandle(m_event);
    if (m_captureClient) m_captureClient->Release();
    if (m_audioClient) m_audioClient->Release();
    if (m_device) m_device->Release();
    CoUninitialize();
#endif
}

AudioBuffer& AudioCapture::buffer() {
    return m_buffer;
}

void AudioCapture::captureThread() {
#ifdef _WIN32
    HANDLE waitArray[1] = {m_event};
    while (m_running.load()) {
        DWORD wait = WaitForMultipleObjects(1, waitArray, FALSE, 2000);
        if (wait != WAIT_OBJECT_0) continue;
        UINT32 packetLength = 0;
        HRESULT hr = m_captureClient->GetNextPacketSize(&packetLength);
        if (FAILED(hr)) continue;
        while (packetLength != 0) {
            BYTE* data;
            UINT32 frames;
            DWORD flags;
            hr = m_captureClient->GetBuffer(&data, &frames, &flags, nullptr, nullptr);
            if (FAILED(hr)) break;
            if (frames > 0) {
                size_t bytes = frames * static_cast<size_t>(m_blockAlign);
                m_buffer.push(data, bytes);
            }
            m_captureClient->ReleaseBuffer(frames);
            hr = m_captureClient->GetNextPacketSize(&packetLength);
            if (FAILED(hr)) break;
        }
    }
#endif
}
