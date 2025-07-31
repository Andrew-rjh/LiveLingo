#include "SystemCapture.h"
#include "MicCapture.h"
#include "WavWriter.h"
#include <iostream>
#include <conio.h>

int main() {
#ifdef _WIN32
    SystemCapture systemCap;
    MicCapture micCap;
    if (!systemCap.start() || !micCap.start()) {
        std::cerr << "Failed to start audio capture" << std::endl;
        return 1;
    }
    std::cout << "Press 's' to save last N seconds, 'q' to quit" << std::endl;
    const double secondsToSave = 10.0; // change as needed
    const int sampleRate = 44100;
    const short channels = 2;
    const short bits = 16;
    while (true) {
        int ch = _getch();
        if (ch == 'q') break;
        if (ch == 's') {
            auto sysData = systemCap.buffer().getLastSamples(secondsToSave, sampleRate * channels * bits / 8);
            auto micData = micCap.buffer().getLastSamples(secondsToSave, sampleRate * channels * bits / 8);
            WavWriter::writeWav("system.wav", sysData, sampleRate, channels, bits);
            WavWriter::writeWav("mic.wav", micData, sampleRate, channels, bits);
            std::cout << "Saved" << std::endl;
        }
    }
    systemCap.stop();
    micCap.stop();
#else
    std::cout << "This example is intended for Windows." << std::endl;
#endif
    return 0;
}
