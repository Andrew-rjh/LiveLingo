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
    const int sysRate = systemCap.sampleRate();
    const short sysChannels = systemCap.channels();
    const short sysBits = systemCap.bitsPerSample();
    const short sysFormat = systemCap.formatType();

    const int micRate = micCap.sampleRate();
    const short micChannels = micCap.channels();
    const short micBits = micCap.bitsPerSample();
    const short micFormat = micCap.formatType();
    while (true) {
        int ch = _getch();
        if (ch == 'q') break;
        if (ch == 's') {
            auto sysData = systemCap.buffer().getLastSamples(secondsToSave, sysRate * sysChannels * sysBits / 8);
            auto micData = micCap.buffer().getLastSamples(secondsToSave, micRate * micChannels * micBits / 8);
            WavWriter::writeWav("system.wav", sysData, sysRate, sysChannels, sysBits, sysFormat);
            WavWriter::writeWav("mic.wav", micData, micRate, micChannels, micBits, micFormat);
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
