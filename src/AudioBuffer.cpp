#include "AudioBuffer.h"
#include <cstring>
#include <algorithm>

AudioBuffer::AudioBuffer(size_t maxSamples)
    : m_capacity(maxSamples) {}

void AudioBuffer::setCapacity(size_t maxSamples) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_buffer.clear();
    m_capacity = maxSamples;
}

void AudioBuffer::push(const void* data, size_t bytes) {
    std::lock_guard<std::mutex> lock(m_mutex);
    const char* ptr = static_cast<const char*>(data);
    for (size_t i = 0; i < bytes; ++i) {
        m_buffer.push_back(ptr[i]);
    }
    while (m_buffer.size() > m_capacity) {
        m_buffer.pop_front();
    }
}

std::vector<char> AudioBuffer::getLastSamples(double seconds, int bytesPerSecond) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    size_t bytesNeeded = static_cast<size_t>(seconds * bytesPerSecond);
    if (bytesNeeded > m_buffer.size()) bytesNeeded = m_buffer.size();
    std::vector<char> out(bytesNeeded);
    auto startIt = m_buffer.end() - bytesNeeded;
    std::copy(startIt, m_buffer.end(), out.begin());
    return out;
}
