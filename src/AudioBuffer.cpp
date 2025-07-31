#include "AudioBuffer.h"
#include <cstring>

AudioBuffer::AudioBuffer(size_t maxSamples)
    : m_buffer(maxSamples), m_capacity(maxSamples) {}

void AudioBuffer::push(const void* data, size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const char* ptr = static_cast<const char*>(data);
    for (size_t i = 0; i < bytes; ++i) {
        m_buffer[m_writePos] = ptr[i];
        m_writePos = (m_writePos + 1) % m_capacity;
    }
}

std::vector<char> AudioBuffer::getLastSamples(double seconds, int bytesPerSecond) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t bytesNeeded = static_cast<size_t>(seconds * bytesPerSecond);
    if (bytesNeeded > m_capacity) bytesNeeded = m_capacity;
    std::vector<char> out(bytesNeeded);
    size_t start = (m_writePos + m_capacity - bytesNeeded) % m_capacity;
    for (size_t i = 0; i < bytesNeeded; ++i) {
        out[i] = m_buffer[(start + i) % m_capacity];
    }
    return out;
}
