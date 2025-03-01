/**
 * @file main.c
 *
 * @brief entry point for nbtgTimer
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include <FreeRTOS.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <task.h>

#include "board.h"
#include "display.h"

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define TASKMGR_TASK_STACK_SIZE 128

//=====================================================================================================================
// Globals
//=====================================================================================================================

//static StaticTask_t tskMgrBuf;
//static StackType_t  tskMgrStack[TASKMGR_TASK_STACK_SIZE * sizeof(StackType_t)];

//=====================================================================================================================
// Functions
//=====================================================================================================================


int main(void)
{
    initBoard();

    initDisplay(MODE_SPI);
}