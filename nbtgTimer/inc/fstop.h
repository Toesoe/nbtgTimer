/**
 * @file  fstop.h
 * @brief fstop calculations using integer math. scaling factor is 1024, 4 digit precision
 *        calculations are performed on milliseconds. maximum resolution is 100ms
 *        avoids using floats
 */

#ifndef _FSTOP_H_
#define _FSTOP_H_

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

typedef enum
{
    FSTOP_FULL,
    FSTOP_HALF,
    FSTOP_THIRD,
    FSTOP_SIXTH
} EFStop_t;

/**
 * @brief calculate single adjusted time
 * @param startTime start time in milliseconds
 * @param reverse if true, will reverse f-stops instead of increase
 * @param resolution f-stop resolution
 * 
 * @return adjusted time
 */
uint32_t adjustTime(uint32_t startTime, bool reverse, EFStop_t resolution);

/**
 * @brief returns table of times for a given start time and number of steps
 * @param startTime start time in milliseconds
 * @param reverse if true, will reverse f-stops instead of increase
 * @param steps number of steps
 * @param resolution f-stop resolution
 * @param[out] pRes table of adjusted times in milliseconds -> is memcpy'd to from inside this function, ensure to free!
 * 
 * @note does not return the start time!
 */
void getTimeTable(uint32_t startTime, bool reverse, size_t steps, EFStop_t resolution, uint32_t *pRes);

// new time = (old time * multiplier) >> 10

#endif //!_FSTOP_H_