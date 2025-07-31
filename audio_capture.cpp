#include <Windows.h>
#include <Audioclient.h>
#include <Mmdeviceapi.h>
#include <stdio.h>
#include <stdint.h>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <string>
#include <iostream>

#pragma comment(lib, "Ole32.lib")
#pragma comment(lib, "OleAut32.lib")

// Ring buffer for PCM data
class AudioRingBuffer {
public:
    AudioRingBuffer(size_t maxFrames, size_t frameSize)
        : mMaxFrames(maxFrames), mFrameSize(frameSize) {
        mBuffer.resize(maxFrames * frameSize);
    }

    void push(const BYTE* data, size_t frames) {
        std::lock_guard<std::mutex> lock(mMutex);
        for (size_t i = 0; i < frames; ++i) {
            memcpy(&mBuffer[(mWriteIndex + i) % mMaxFrames * mFrameSize],
                   data + i * mFrameSize, mFrameSize);
        }
        mWriteIndex = (mWriteIndex + frames) % mMaxFrames;
        if (mSize < mMaxFrames) {
            mSize = std::min(mSize + frames, mMaxFrames);
        }
    }

    std::vector<BYTE> getLast(size_t frames) {
        std::lock_guard<std::mutex> lock(mMutex);
        if (frames > mSize) frames = mSize;
        std::vector<BYTE> out(frames * mFrameSize);
        size_t start = (mWriteIndex + mMaxFrames - frames) % mMaxFrames;
        for (size_t i = 0; i < frames; ++i) {
            memcpy(&out[i * mFrameSize],
                   &mBuffer[(start + i) % mMaxFrames * mFrameSize],
                   mFrameSize);
        }
        return out;
    }

private:
    std::vector<BYTE> mBuffer;
    size_t mWriteIndex = 0;
    size_t mSize = 0;
    size_t mMaxFrames;
    size_t mFrameSize;
    std::mutex mMutex;
};

class AudioCapture {
public:
    AudioCapture(bool loopback)
        : mLoopback(loopback) {}

    bool init() {
        HRESULT hr;
        hr = CoInitializeEx(NULL, COINIT_MULTITHREADED);
        if (FAILED(hr)) return false;
        IMMDeviceEnumerator* pEnum = NULL;
        hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
                              __uuidof(IMMDeviceEnumerator), (void**)&pEnum);
        if (FAILED(hr)) return false;
        hr = pEnum->GetDefaultAudioEndpoint(mLoopback ? eRender : eCapture,
                                            mLoopback ? eConsole : eCommunications,
                                            &mDevice);
        if (FAILED(hr)) return false;
        hr = mDevice->Activate(__uuidof(IAudioClient), CLSCTX_ALL, NULL,
                               (void**)&mClient);
        if (FAILED(hr)) return false;
        WAVEFORMATEX* wf = NULL;
        hr = mClient->GetMixFormat(&wf);
        if (FAILED(hr)) return false;
        mFormat = *wf;
        CoTaskMemFree(wf);
        REFERENCE_TIME hnsBufferDuration = 10000000; // 1 second
        DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
        if (mLoopback) flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
        hr = mClient->Initialize(AUDCLNT_SHAREMODE_SHARED, flags,
                                 hnsBufferDuration, 0, &mFormat, NULL);
        if (FAILED(hr)) return false;
        hr = mClient->GetService(__uuidof(IAudioCaptureClient),
                                 (void**)&mCapture);
        if (FAILED(hr)) return false;
        mEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
        if (!mEvent) return false;
        hr = mClient->SetEventHandle(mEvent);
        if (FAILED(hr)) return false;
        UINT32 bufferFrameCount;
        hr = mClient->GetBufferSize(&bufferFrameCount);
        if (FAILED(hr)) return false;
        mRing = std::make_unique<AudioRingBuffer>(bufferFrameCount * 36000, mFormat.nBlockAlign); // up to 600min (approx)
        return true;
    }

    void start() {
        mRunning = true;
        mThread = std::thread(&AudioCapture::captureLoop, this);
    }

    void stop() {
        mRunning = false;
        if (mThread.joinable()) mThread.join();
        if (mClient) mClient->Stop();
    }

    void saveRecent(int seconds, const std::wstring& filename) {
        size_t frames = seconds * mFormat.nSamplesPerSec;
        auto data = mRing->getLast(frames);
        // write to wav
        FILE* f = _wfopen(filename.c_str(), L"wb");
        if (!f) return;
        writeWavHeader(f, data.size() / mFormat.nBlockAlign);
        fwrite(data.data(), 1, data.size(), f);
        fclose(f);
    }

    WAVEFORMATEX getFormat() const { return mFormat; }

private:
    void writeWavHeader(FILE* f, DWORD frames) {
        DWORD dataSize = frames * mFormat.nBlockAlign;
        DWORD fmtSize = sizeof(WAVEFORMATEX);
        fwrite("RIFF", 1, 4, f);
        DWORD riffSize = dataSize + 36;
        fwrite(&riffSize, 4, 1, f);
        fwrite("WAVE", 1, 4, f);
        fwrite("fmt ", 1, 4, f);
        fwrite(&fmtSize, 4, 1, f);
        fwrite(&mFormat, 1, sizeof(WAVEFORMATEX), f);
        fwrite("data", 1, 4, f);
        fwrite(&dataSize, 4, 1, f);
    }

    void captureLoop() {
        HRESULT hr = mClient->Start();
        if (FAILED(hr)) return;
        HANDLE waitArray[1] = { mEvent };
        while (mRunning) {
            DWORD result = WaitForMultipleObjects(1, waitArray, FALSE, 1000);
            if (result != WAIT_OBJECT_0) continue;
            UINT32 packetSize = 0;
            hr = mCapture->GetNextPacketSize(&packetSize);
            if (FAILED(hr)) break;
            while (packetSize != 0) {
                BYTE* data = NULL;
                UINT32 frames = 0;
                DWORD flags = 0;
                hr = mCapture->GetBuffer(&data, &frames, &flags, NULL, NULL);
                if (FAILED(hr)) break;
                if (frames > 0) {
                    mRing->push(data, frames);
                }
                hr = mCapture->ReleaseBuffer(frames);
                if (FAILED(hr)) break;
                hr = mCapture->GetNextPacketSize(&packetSize);
                if (FAILED(hr)) break;
            }
        }
    }

    bool mLoopback;
    IMMDevice* mDevice = nullptr;
    IAudioClient* mClient = nullptr;
    IAudioCaptureClient* mCapture = nullptr;
    WAVEFORMATEX mFormat{};
    HANDLE mEvent = NULL;
    std::thread mThread;
    std::atomic<bool> mRunning{false};
    std::unique_ptr<AudioRingBuffer> mRing;
};

int wmain() {
    AudioCapture systemCapture(true);
    AudioCapture micCapture(false);
    if (!systemCapture.init() || !micCapture.init()) {
        std::cerr << "Failed to init audio" << std::endl;
        return -1;
    }
    systemCapture.start();
    micCapture.start();

    std::cout << "Press s to save system audio, m to save mic, q to quit.\n";
    while (true) {
        char c;
        std::cin >> c;
        if (c == 'q') break;
        if (c == 's' || c == 'm') {
            std::cout << "Seconds to capture (max 36000): ";
            int sec;
            std::cin >> sec;
            if (sec < 0) sec = 0;
            if (sec > 36000) sec = 36000;
            auto now = std::chrono::system_clock::now();
            auto t = std::chrono::system_clock::to_time_t(now);
            wchar_t filename[64];
            if (c == 's') {
                swprintf(filename, 64, L"system_%lld.wav", (long long)t);
                systemCapture.saveRecent(sec, filename);
            } else {
                swprintf(filename, 64, L"mic_%lld.wav", (long long)t);
                micCapture.saveRecent(sec, filename);
            }
            std::cout << "Saved " << sec << " seconds to file." << std::endl;
        }
    }
    systemCapture.stop();
    micCapture.stop();
    return 0;
}

