#ifndef SYSTEM_CAPTURE_H
#define SYSTEM_CAPTURE_H

#include "AudioCapture.h"

class SystemCapture : public AudioCapture {
public:
    SystemCapture() : AudioCapture(true) {}
};

#endif // SYSTEM_CAPTURE_H
