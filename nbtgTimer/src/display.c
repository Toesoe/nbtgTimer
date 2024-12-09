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
#include "images.h"

#include "board.h"

#include <string.h>

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

static uint8_t g_framebuffer[(SSD1306_WIDTH * SSD1306_HEIGHT / 8) + 1] = { 0x40, 0 };
static bool    dmaIsEnabled = false;

static bool    dmaInProgress = false;
static bool    nonDMAInProgress = false;

static SI2CTransfer_t g_i2cTransfer = { .address = SSD1306_ADDR };

uint8_t SSD1309_INIT_SEQ[] = {
    SSD1309_DISPLAY_OFF,
    SSD1309_SET_DISPLAY_CLOCK_DIV, 0x80,
    SSD1309_SET_MULTIPLEX, (SSD1306_HEIGHT - 1),
    SSD1309_SET_DISPLAY_OFFSET, 0x00,
    SSD1309_SET_START_LINE, 0x00,
    SSD1309_CHARGE_PUMP, 0x14,
    SSD1309_MEMORY_MODE, 0x00,      // 0x0 is horizontal, 0x1 is vertical
    SSD1309_SEG_REMAP_NORMAL,       // Segment Remap Flip
    SSD1309_COM_SCAN_DEC,           // Com Scan DEC
    SSD1309_SET_COM_PINS, 0x12,
    SSD1309_SET_CONTRAST, 0xFF,
    SSD1309_SET_PRECHARGE, 0xC2,
    SSD1309_SET_VCOM_DESELECT, 0x40,
    SSD1309_DISPLAY_ALL_ON_RESUME,
    SSD1309_NORMAL_DISPLAY,
    SSD1309_DEACTIVATE_SCROLL,
    SSD1309_DISPLAY_ON
};

//=====================================================================================================================
// Static protos
//=====================================================================================================================

static void dispConfigureDMA(void);
static void dispToggleDMA(bool);

static void dispWriteCommand(uint8_t);
static void dispWriteFbuf(void);
static void dispResetPointers(void);
static void dispSyncFramebuffer(void);

static void dispWriteMulti(uint8_t *, size_t);
static void dispI2CDMACallback(bool);
static void dispI2RegularCallback(bool);

//=====================================================================================================================
// Functions
//=====================================================================================================================

static void subInit(void)
{
    dispWriteCommand(0xAE); /* Display off */

    dispWriteCommand(0x20); /* Set Memory Addressing Mode */   
    dispWriteCommand(0x10); /* 00,Horizontal Addressing Mode; 01,Vertical Addressing Mode; */
                                /* 10,Page Addressing Mode (RESET); 11,Invalid */

    dispWriteCommand(0xB0); /*Set Page Start Address for Page Addressing Mode, 0-7 */

#ifdef SSD1309_MIRROR_VERT
    dispWriteCommand(0xC0); /* Mirror vertically */
#else
    dispWriteCommand(0xC8); /* Set COM Output Scan Direction */
#endif

    dispWriteCommand(0x00); /*---set low column address  */
    dispWriteCommand(0x10); /*---set high column address */

    dispWriteCommand(0x40); /*--set start line address - CHECK */

    dispWriteCommand(0x81); /*--set contrast control register - CHECK */
    dispWriteCommand(0xFF);

#ifdef SSD1309_MIRROR_HORIZ
    dispWriteCommand(0xA0); /* Mirror horizontally */
#else
    dispWriteCommand(0xA1); /* --set segment re-map 0 to 127 - CHECK */
#endif

#ifdef SSD1309_INVERSE_COLOR
    dispWriteCommand(0xA7); /*--set inverse color */
#else
    dispWriteCommand(0xA6); /*--set normal color */
#endif

/* Set multiplex ratio. */
#if (SSD1309_HEIGHT == 128)
    /* Found in the Luma Python lib for SH1106. */
    ssd1306_WriteCommand(0xFF);
#else
    dispWriteCommand(0xA8); /*--set multiplex ratio(1 to 64) - CHECK */
#endif

    dispWriteCommand(0x3F);


    dispWriteCommand(0xA4); /* 0xA4, Output follows RAM content;0xa5,Output ignores RAM content */

    dispWriteCommand(0xD3); /*-set display offset - CHECK */
    dispWriteCommand(0x00); /*-not offset */

    dispWriteCommand(0xD5); /*--set display clock divide ratio/oscillator frequency */
    dispWriteCommand(0xF0); /*--set divide ratio */

    dispWriteCommand(0xD9); /*--set pre-charge period */
    dispWriteCommand(0x22); /*			  */

    dispWriteCommand(0xDA); /*--set com pins hardware configuration - CHECK */
    dispWriteCommand(0x12);

    dispWriteCommand(0xDB); /*--set vcomh */
    dispWriteCommand(0x20); /* 0x20, 0.77xVcc */ 

    dispWriteCommand(0x8D); /*--set DC-DC enable */
    dispWriteCommand(0x14); /*                   */
    dispWriteCommand(0xAF); /*--turn on SSD1309 panel */
}


void initDisplay(void)
{
    i2cRegisterCallback(dispI2RegularCallback);

    i2cInitDisplayDMA(dispI2CDMACallback);

    for (size_t i = 0; i < sizeof(SSD1309_INIT_SEQ) / sizeof(SSD1309_INIT_SEQ[0]); i++)
    {
        dispWriteCommand(SSD1309_INIT_SEQ[i]);
    }

    dispResetPointers();

    //subInit();

    //dispWriteCommand(0xA5);

    dispToggleDMA(true);
    memcpy(&g_framebuffer[1], logo, sizeof(logo) / sizeof(logo[0]));
    dispSyncFramebuffer();

    while (nonDMAInProgress) {};

    memset(&g_framebuffer[1], 0xFF, 1024);

    dispSyncFramebuffer();

    while (dmaInProgress) {};

    __asm("nop");

    //memcpy(&g_framebuffer[1], logo, sizeof(logo) / sizeof(logo[0]));

    //dispSyncFramebuffer();

    //while (dmaInProgress) {};

    while(true);
}

void dispReset(void)
{

}

static void dispToggleDMA(bool enable)
{
    dmaIsEnabled = enable;
}

static void dispWriteCommand(uint8_t cmd)
{
    uint8_t buf[] = { 0x00, cmd };

    nonDMAInProgress = true;
    g_i2cTransfer.pBuffer = buf;
    g_i2cTransfer.len = 2;

    i2cTransmit(I2C2, &g_i2cTransfer);

    while (nonDMAInProgress) {}; // await cmd transaction finish
}

static void dispResetPointers(void)
{
    uint8_t buf[] = { 0x00, SSD1309_COLUMN_ADDR, 0x00, 0x7F };
    uint8_t buf2[] = { 0x00, SSD1309_PAGE_ADDR, 0x00, 0x07 };

    nonDMAInProgress = true;
    g_i2cTransfer.pBuffer = buf;
    g_i2cTransfer.len = 4;

    i2cTransmit(I2C2, &g_i2cTransfer);

    while (nonDMAInProgress) {};

    nonDMAInProgress = true;
    g_i2cTransfer.pBuffer = buf2;
    g_i2cTransfer.len = 4;

    i2cTransmit(I2C2, &g_i2cTransfer);

    while (nonDMAInProgress) {};
}

/**
 * @brief yeet the framebuffer to the display using DMA
 * 
 */
static void dispSyncFramebuffer(void)
{
    if (!dmaIsEnabled) return;

    dmaInProgress = true;
    g_i2cTransfer.pBuffer = g_framebuffer;
    g_i2cTransfer.len = 1025;

    i2cTransferDisplayDMA(&g_i2cTransfer);
}

static void dispWriteFbuf(void)
{
    nonDMAInProgress = true;
    g_i2cTransfer.pBuffer = g_framebuffer;
    g_i2cTransfer.len = 1025;

    i2cTransmit(I2C2, &g_i2cTransfer);
}

static void dispI2CDMACallback(bool isComplete)
{
    dmaInProgress = false;
}

static void dispI2RegularCallback(bool isComplete)
{
    nonDMAInProgress = false;
}