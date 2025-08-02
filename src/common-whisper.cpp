#include "common-whisper.h"
#include <cstdio>

std::string to_timestamp(int64_t t, bool comma) {
    int64_t msec = t * 10;
    int64_t hr = msec / (1000 * 60 * 60);
    msec -= hr * (1000 * 60 * 60);
    int64_t min = msec / (1000 * 60);
    msec -= min * (1000 * 60);
    int64_t sec = msec / 1000;
    msec -= sec * 1000;

    char buf[32];
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d%s%03d", (int) hr, (int) min, (int) sec, comma ? "," : ".", (int) msec);
    return std::string(buf);
}

