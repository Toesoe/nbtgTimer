/**
 * @file  fstop.c
 * @brief fstop calculations using integer math. scaling factor is 1024, 4 digit precision
 *        calculations are performed on milliseconds. maximum resolution is 100ms
 *        avoids using floats
 */

#include "fstop.h"

#include <string.h>

#define SCALE_FACTOR 1024

// precomputed multipliers
#define PLUS_ONE_SIXTH 1149
#define PLUS_ONE_THIRD 1293
#define PLUS_HALF      1448
#define PLUS_FULL      2048

#define MINUS_ONE_SIXTH 913
#define MINUS_ONE_THIRD 813
#define MINUS_HALF      724
#define MINUS_FULL      512

uint32_t adjustTime(uint32_t startTime, bool reverse, EFStop_t resolution)
{
    uint32_t newTime = 0;

    switch (resolution)
    {
        case FSTOP_FULL:
            newTime = (startTime * (reverse ? MINUS_FULL : PLUS_FULL)) >> 10;
            break;
        case FSTOP_HALF:
            newTime = (startTime * (reverse ? MINUS_HALF : PLUS_HALF)) >> 10;
            break;
        case FSTOP_THIRD:
            newTime = (startTime * (reverse ? MINUS_ONE_THIRD : PLUS_ONE_THIRD)) >> 10;
            break;
        case FSTOP_SIXTH:
            newTime = (startTime * (reverse ? MINUS_ONE_SIXTH : PLUS_ONE_SIXTH)) >> 10;
            break;
        default:
            break;
    }

    return newTime;
}

void getTimeTable(uint32_t startTime, bool reverse, size_t steps, EFStop_t resolution, uint32_t *pRes)
{
    uint32_t newTimes[steps + 1] = { startTime }; // +1 for start time

    for (size_t i = 0; i < steps; i++)
    {
        newTimes[i + 1] = adjustTime(newTimes[i], reverse, resolution);
    }

    memcpy(pRes, &newTimes[1], steps * sizeof(uint32_t)); // don't copy the start time
}