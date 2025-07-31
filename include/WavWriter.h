#ifndef WAV_WRITER_H
#define WAV_WRITER_H

#include <string>
#include <vector>

class WavWriter {
public:
    static bool writeWav(const std::string& path,
                         const std::vector<char>& data,
                         int sampleRate,
                         short channels,
                         short bitsPerSample);
};

#endif // WAV_WRITER_H
