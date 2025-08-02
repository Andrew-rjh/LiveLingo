#pragma comment(lib, "whisper.lib")
#pragma comment(lib, "ggml.lib")
#pragma comment(lib, "ggml-base.lib")
#pragma comment(lib, "ggml-cpu.lib")
#pragma comment(lib, "ggml-cuda.lib")
#pragma comment(lib, "cuda.lib")
#pragma comment(lib, "CompilerIdCUDA.lib")
#pragma comment(lib, "cudart.lib")
#pragma comment(lib, "cublas.lib")

#include "SystemCapture.h"
#include "MicCapture.h"
#include <iostream>
#include "WhisperBridge.h"
#include <thread>
#include <atomic>
#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#include <locale>
#endif

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    std::setlocale(LC_ALL, ".65001");

    SystemCapture systemCap;
    MicCapture micCap;
    if (!systemCap.start() || !micCap.start()) {
        std::cerr << "Failed to start audio capture" << std::endl;
        return 1;
    }

    if (!WhisperBridge::init("models/ggml-medium.bin")) {
        std::cerr << "Failed to initialize model" << std::endl;
        return 1;
    }

    const double chunkSeconds = 5.0; // duration of each transcription chunk
    const int sysRate = systemCap.sampleRate();
    const short sysChannels = systemCap.channels();
    const short sysBits = systemCap.bitsPerSample();
    const std::string language = "ko"; // empty -> auto-detect

    std::atomic<bool> running(true);
    std::thread worker([&]() {
        while (running.load()) {
            auto sysData = systemCap.getLastSamples(chunkSeconds);
            std::string text = WhisperBridge::transcribeBuffer(sysData, sysRate, sysChannels, sysBits, language);
            if (!text.empty()) {
                std::cout << text << std::endl;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(chunkSeconds * 1000)));
        }
    });

    std::cout << "Press 'q' to quit" << std::endl;
    while (true) {
        int ch = _getch();
        if (ch == 'q') break;
    }

    running = false;
    worker.join();
    systemCap.stop();
    micCap.stop();
    WhisperBridge::cleanup();
#else
    std::cout << "This example is intended for Windows." << std::endl;
#endif
    return 0;
}
