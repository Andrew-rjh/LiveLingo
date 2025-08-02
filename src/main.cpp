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
#include <chrono>
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

    const std::string language = "ko"; // empty -> auto-detect, e.g. "en" or "ko"
    WhisperBridge::WhisperStream streamer("models/ggml-medium.bin", language);
    if (_kbhit()) _getch(); // clear any buffered input
    std::cout << "Streaming transcription. Press 'q' to quit." << std::endl;

    const double chunkSeconds = 5.0; // transcribe every 5 seconds
    while (true) {
        if (_kbhit()) {
            int ch = _getch();
            if (ch == 'q') break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(chunkSeconds * 1000)));
        auto micData = micCap.getLastSamples(chunkSeconds);
        std::string text = streamer.transcribe(micData,
                                               micCap.sampleRate(),
                                               micCap.channels(),
                                               micCap.bitsPerSample(),
                                               micCap.formatType());
        if (!text.empty()) {
            std::cout << text << std::endl;
        }
    }
    systemCap.stop();
    micCap.stop();
#else
    std::cout << "This example is intended for Windows." << std::endl;
#endif
    return 0;
}
