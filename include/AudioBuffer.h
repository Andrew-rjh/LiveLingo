#ifndef AUDIO_BUFFER_H
#define AUDIO_BUFFER_H

#include <vector>
#include <deque>
#include <mutex>

class AudioBuffer {
public:
    AudioBuffer(size_t maxSamples = 0);
    // Adjust the internal capacity. This clears existing contents.
    void setCapacity(size_t maxSamples);
    void push(const void* data, size_t bytes);
    std::vector<char> getLastSamples(double seconds, int bytesPerSecond) const;

private:
    mutable std::mutex m_mutex;
    std::deque<char> m_buffer;
    size_t m_capacity = 0;
};

#endif // AUDIO_BUFFER_H
