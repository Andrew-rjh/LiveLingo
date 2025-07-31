#include "AudioBuffer.h"
#include <cstring>
#include <algorithm>

AudioBuffer::AudioBuffer(size_t maxSamples)
    : m_capacity(maxSamples), m_buffer(maxSamples) {}

void AudioBuffer::setCapacity(size_t maxSamples) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_buffer.assign(maxSamples, 0);
    m_capacity = maxSamples;
    m_writePos = 0;
    m_size = 0;
}

void AudioBuffer::push(const void* data, size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const char* ptr = static_cast<const char*>(data);
    if (m_capacity == 0) return;
    for (size_t i = 0; i < bytes; ++i) {
        m_buffer[m_writePos] = ptr[i];
        m_writePos = (m_writePos + 1) % m_capacity;
        if (m_size < m_capacity) ++m_size;
    }
}

std::vector<char> AudioBuffer::getLastSamples(double seconds, int bytesPerSecond) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t bytesNeeded = static_cast<size_t>(seconds * bytesPerSecond);
    if (bytesNeeded > m_size) bytesNeeded = m_size;
    std::vector<char> out(bytesNeeded);
    size_t startPos = (m_writePos + m_capacity - bytesNeeded) % m_capacity;
    if (startPos + bytesNeeded <= m_capacity) {
        std::copy_n(m_buffer.begin() + startPos, bytesNeeded, out.begin());
    } else {
        size_t firstPart = m_capacity - startPos;
        std::copy_n(m_buffer.begin() + startPos, firstPart, out.begin());
        std::copy_n(m_buffer.begin(), bytesNeeded - firstPart, out.begin() + firstPart);
    }
    return out;
}
