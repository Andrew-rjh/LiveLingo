#pragma once

#include <vector>

// Base interface for asynchronous audio capture devices
class audio_capture {
public:
    virtual ~audio_capture() = default;
    virtual bool init(int capture_id, int sample_rate) = 0;
    virtual bool resume() = 0;
    virtual bool pause() = 0;
    virtual bool clear() = 0;
    virtual void get(int ms, std::vector<float>& audio) = 0;
};

