#ifndef MIC_CAPTURE_H
#define MIC_CAPTURE_H

#include "AudioCapture.h"

class MicCapture : public AudioCapture {
public:
    MicCapture() : AudioCapture(false) {}
};

#endif // MIC_CAPTURE_H
