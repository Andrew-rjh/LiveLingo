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
#include "WavWriter.h"
#include <iostream>
#include "WhisperBridge.h"
#include <thread>
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
    std::cout << "Press 's' to save last N seconds, 'q' to quit" << std::endl;
    const double secondsToSave = 10.0; // change as needed
    const int sysRate = systemCap.sampleRate();
    const short sysChannels = systemCap.channels();
    const short sysBits = systemCap.bitsPerSample();
    const short sysFormat = systemCap.formatType();

    const int micRate = micCap.sampleRate();
    const short micChannels = micCap.channels();
    const short micBits = micCap.bitsPerSample();
    const short micFormat = micCap.formatType();
    const std::string language = "ko"; // empty -> auto-detect, e.g. "en" or "ko"
    while (true) {
        int ch = _getch();
        if (ch == 'q') break;
        if (ch == 's') {
            auto sysData = systemCap.getLastSamples(secondsToSave);
            auto micData = micCap.getLastSamples(secondsToSave);
            WavWriter::writeWav("system.wav", sysData, sysRate, sysChannels, sysBits, sysFormat);
            WavWriter::writeWav("mic.wav", micData, micRate, micChannels, micBits, micFormat);
            std::cout << "Saved" << std::endl;
            //WhisperBridge::transcribeFile("models/ggml-base.bin", "mic.wav", language);//
            WhisperBridge::transcribeFile("models/ggml-medium.bin", "mic.wav", language);//ggml-medium
        }
    }
    systemCap.stop();
    micCap.stop();
#else
    std::cout << "This example is intended for Windows." << std::endl;
#endif
    return 0;
}
