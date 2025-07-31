#ifndef MULAW_H
#define MULAW_H

#include <cstdint>

unsigned char linearToMuLaw(int16_t sample);
int16_t muLawToLinear(unsigned char mu);

#endif // MULAW_H
