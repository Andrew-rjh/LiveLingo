#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <vector>
#include <mutex>

class AudioBuffer {
public:
    AudioBuffer(size_t maxSamples = 0);
    void push(const void* data, size_t bytes);
    std::vector<char> getLastSamples(double seconds, int bytesPerSecond) const;

private:
    mutable std::mutex m_mutex;
    std::vector<char> m_buffer;
    size_t m_writePos = 0;
    size_t m_capacity = 0;
};

#endif // AUDIO_BUFFER_H
