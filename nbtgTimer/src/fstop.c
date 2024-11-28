/**
 * @file  fstop.c
 * @brief fstop calculations using integer math. scaling factor is 1024, 4 digit precision
 *        calculations are performed on milliseconds. maximum resolution is 100ms
 *        avoids using floats
 * 
 * TODO: test strip mode, either 1/6th or 1/3rd : -1, -2/3, -1/3, B, etc
 */

#include "fstop.h"

#include <string.h>

#define SCALE_FACTOR 1024
#define MAX_RES_MS   100

// precomputed multipliers
#define PLUS_ONE_SIXTH 1149
#define PLUS_ONE_THIRD 1293
#define PLUS_HALF      1448
#define PLUS_FULL      2048

#define MINUS_ONE_SIXTH 913
#define MINUS_ONE_THIRD 813
#define MINUS_HALF      724
#define MINUS_FULL      512

// statics
static void reverseArray(uint32_t *, size_t);

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

    uint32_t mod = newTime % MAX_RES_MS;

    if (mod > MAX_RES_MS / 2)
    {
        newTime = (newTime * 2) - mod;
    }
    else if (mod != 0)
    {
        newTime = newTime - mod;
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

void genererateTestStrip(uint32_t baseTime, size_t steps, EFStop_t resolution, uint32_t *pRes)
{
    uint32_t testStripTime[(steps * 2) + 1] = { 0 };
    uint32_t tmp[steps];

    getTimeTable(baseTime, true, steps, resolution, &tmp);
    reverseArray(tmp, steps);
    memcpy(testStripTime, tmp, steps * sizeof(uint32_t));

    testStripTime[steps] = baseTime;

    getTimeTable(baseTime, false, steps, resolution, &tmp);
    memcpy(&testStripTime[steps + 1], tmp, steps * sizeof(uint32_t));

    memcpy(pRes, testStripTime, ((steps * 2) + 1) * sizeof(uint32_t));
}

static void reverseArray(uint32_t *array, size_t size)
{
    size_t i = 0;
    size_t j = size - 1;

    for (size_t i = 0; i < size/2; i++)
    {
        uint32_t temp = array[i];
        array[i] = array[size - 1 - i];
        array[size - 1 - i] = temp;
    }
}