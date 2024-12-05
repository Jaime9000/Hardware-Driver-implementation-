#ifndef TIMING_H
#define TIMING_H

#include <stdint.h>
#include <windows.h>

static inline uint64_t get_current_time_ms(void) {
    LARGE_INTEGER frequency;
    LARGE_INTEGER count;
    
    QueryPerformanceFrequency(&frequency);
    QueryPerformanceCounter(&count);
    
    return (count.QuadPart * 1000) / frequency.QuadPart;
}

#endif // TIMING_H