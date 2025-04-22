/**
 * @file  fstop.c
 * @brief fstop calculations using integer math. scaling factor is 1024, 4 digit precision
 *        calculations are performed on milliseconds. maximum resolution is 100ms
 *        avoids using floats
 * 
 * TODO: test strip mode, either 1/6th or 1/3rd : -1, -2/3, -1/3, B, etc
 */

 //=====================================================================================================================
// Includes
//=====================================================================================================================

#include "fstop.h"

#include <string.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define SCALE_FACTOR 1024
#define MAX_RES_MS   100

// precomputed multipliers
#define PLUS_ONE_SIXTH 1149    // 1.1225 × 1024 ≈ 1149.4
#define PLUS_ONE_THIRD 1290    // 1.2599 × 1024 ≈ 1290.1
#define PLUS_HALF 1448         // 1.4142 × 1024 ≈ 1448.2
#define PLUS_FULL 2048         // 2.0 × 1024 = 2048

#define MINUS_ONE_SIXTH 891    // 0.8700 × 1024 ≈ 890.9
#define MINUS_ONE_THIRD 814    // 0.7943 × 1024 ≈ 813.5
#define MINUS_HALF 724         // 0.7071 × 1024 ≈ 724.1
#define MINUS_FULL 512         // 0.5 × 1024 = 512

//=====================================================================================================================
// Constants
//=====================================================================================================================

//=====================================================================================================================
// Types
//=====================================================================================================================

//=====================================================================================================================
// Globals
//=====================================================================================================================

//=====================================================================================================================
// Static protos
//=====================================================================================================================

static void reverseArray(uint32_t *, size_t);

//=====================================================================================================================
// Functions
//=====================================================================================================================

uint32_t calculateNextFStop(uint32_t startTime, bool reverse, EFStop_t resolution)
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
        newTime = newTime + (MAX_RES_MS - mod);  // Round up
    }
    else if (mod != 0)
    {
        newTime = newTime - mod;  // Round down
    }

    return newTime;
}

void getTimeTable(uint32_t startTime, bool reverse, size_t steps, EFStop_t resolution, uint32_t *pRes)
{
    uint32_t currentTime = startTime;
    
    for (size_t i = 0; i < steps; i++)
    {
        // Calculate the next time value based on the current time
        currentTime = calculateNextFStop(currentTime, reverse, resolution);
        
        // Store the result
        pRes[i] = currentTime;
    }
}

void genererateTestStrip(uint32_t baseTime, size_t steps, EFStop_t resolution, uint32_t *pRes)
{
    uint32_t testStripTime[(steps * 2) + 1];
    uint32_t tmp[steps];

    // calculate lower set of times (so shorter than baseTime)
    getTimeTable(baseTime, true, steps, resolution, &tmp[0]);
    reverseArray(tmp, steps);
    memcpy(testStripTime, tmp, steps * sizeof(uint32_t));

    // original time goes in the middle
    testStripTime[steps] = baseTime;

    // calculate higher set of times
    getTimeTable(baseTime, false, steps, resolution, &tmp[0]);
    memcpy(&testStripTime[steps + 1], tmp, steps * sizeof(uint32_t));

    memcpy(pRes, testStripTime, ((steps * 2) + 1) * sizeof(uint32_t));
}

static void reverseArray(uint32_t *array, size_t size)
{
    for (size_t i = 0; i < size/2; i++)
    {
        uint32_t temp = array[i];
        array[i] = array[size - 1 - i];
        array[size - 1 - i] = temp;
    }
}