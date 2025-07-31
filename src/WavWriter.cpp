#include "WavWriter.h"
#include <fstream>
#include <cstdint>

struct WavHeader {
    char riff[4] = {'R','I','F','F'};
    uint32_t overall_size;
    char wave[4] = {'W','A','V','E'};
    char fmt_chunk_marker[4] = {'f','m','t',' '};
    uint32_t length_of_fmt = 16;
    uint16_t format_type = 1;
    uint16_t channels;
    uint32_t sample_rate;
    uint32_t byterate;
    uint16_t block_align;
    uint16_t bits_per_sample;
    char data_chunk_header[4] = {'d','a','t','a'};
    uint32_t data_size;
};

bool WavWriter::writeWav(const std::string& path,
                         const std::vector<char>& data,
                         int sampleRate,
                         short channels,
                         short bitsPerSample) {
    WavHeader header;
    header.data_size = data.size();
    header.overall_size = header.data_size + sizeof(WavHeader) - 8;
    header.channels = channels;
    header.sample_rate = sampleRate;
    header.byterate = sampleRate * channels * bitsPerSample / 8;
    header.block_align = channels * bitsPerSample / 8;
    header.bits_per_sample = bitsPerSample;

    std::ofstream ofs(path, std::ios::binary);
    if (!ofs) return false;
    ofs.write(reinterpret_cast<const char*>(&header), sizeof(header));
    ofs.write(data.data(), data.size());
    return true;
}
