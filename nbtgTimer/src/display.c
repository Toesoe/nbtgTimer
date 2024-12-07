/**
 * @file display.c
 *
 * @brief display routines for nbtgTimer
 * SSD1309 using DMA/I2C
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "display.h"

#include "board.h"

#include <stm32g0xx_ll_system.h>

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define SSD1306_WIDTH           128
#define SSD1306_HEIGHT          64

#define SSD1306_ADDR            0x78

#define IS_MIRRORED_VERT             0
#define IS_MIRRORED_HOR              1

//=====================================================================================================================
// Globals
//=====================================================================================================================

static uint8_t g_framebuffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];
static bool    dmaIsEnabled = false;

static bool    dmaInProgress = false;

//=====================================================================================================================
// Static protos
//=====================================================================================================================

static void dispWriteCommand(uint8_t);
static void dispWriteMulti(uint8_t *, size_t);
static void dmaTxCompleteCallback(void);
static void dmaTxErrCallback(void);

//=====================================================================================================================
// Functions
//=====================================================================================================================

void initDisplay(void)
{
    dispWriteCommand(SSD1309_DISPLAY_OFF);
    dispWriteCommand(SSD1309_MEMORY_ADDR_MODE);
    dispWriteCommand(0x10); // page addressing mode
    dispWriteCommand(SSD1309_SET_PAGE_START);

#if IS_MIRRORED_VERT
    dispWriteCommand(SSD1309_COM_SCAN_DIR);
#else
    dispWriteCommand(SSD1309_COM_SCAN_DIR_OP);
#endif

    dispWriteCommand(0x00); // low column address
    dispWriteCommand(0x10); // high column address
    dispWriteCommand(SSD1309_SET_START_LINE);
    dispWriteCommand(SSD1309_SET_CONTRAST);
    dispWriteCommand(0xFF); // contrast value

#if IS_MIRRORED_HOR
    dispWriteCommand(SSD1309_SEG_REMAP);
#else
    dispWriteCommand(SSD1309_SEG_REMAP_OP);
#endif

    dispWriteCommand(SSD1309_DIS_NORMAL); // normal mode: 0 in RAM = off

    dispWriteCommand(SSD1309_SET_MUX_RATIO);
    dispWriteCommand(0x3F); // 64MUX for 64 pixels high

    dispWriteCommand(SSD1309_DISPLAY_OFFSET);
    dispWriteCommand(0x00); // no offset

    dispWriteCommand(SSD1309_SET_OSC_FREQ);
    dispWriteCommand(0xF0); // divide ratio 1, oscfreq 15

    dispWriteCommand(SSD1309_SET_PRECHARGE);
    dispWriteCommand(0x22); // number of DCLK, where RESET equals 2 DCLKs

    dispWriteCommand(SSD1309_COM_PIN_CONF);
    dispWriteCommand(0x12); // sequential com pin configuration for 64 pixels high

    dispWriteCommand(SSD1309_VCOM_DESELECT);
    dispWriteCommand(0x20); // VCOM deselect level

    dispWriteCommand(SSD1309_SET_CHAR_REG);
    dispWriteCommand(0x14); // enable charge pump
    dispWriteCommand(SSD1309_DISPLAY_ON);
}

void dispReset(void)
{

}

void dispEnableDMA(void)
{
    initDisplayI2CDMA(SSD1306_ADDR, SSD1306_WIDTH * SSD1306_HEIGHT / 8, (uint32_t)g_framebuffer, dmaTxCompleteCallback, dmaTxErrCallback);
}

static void dispWriteCommand(uint8_t data)
{
    uint8_t buf[2] = {0x00, data};
    I2CTransmit(I2C2, SSD1306_ADDR, &data, 2);
}

/**
 * @brief yeet the framebuffer to the display using DMA
 * 
 */
static void dispSyncFramebuffer(void)
{
    dmaInProgress = true;
    i2cTransferDMA();
}

static void dmaTxCompleteCallback(void)
{
    dmaInProgress = false;
}

static void dmaTxErrCallback(void)
{
    dmaInProgress = false;
}