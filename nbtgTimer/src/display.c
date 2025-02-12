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

#define SSD1309_WIDTH             128
#define SSD1309_HEIGHT            64

#define SSD1309_PAGE_SIZE_BYTES   128
#define SSD1309_ROW_SIZE_BYTES    16
#define SSD1309_GDDRAM_SIZE_BYTES 1024

#define SSD1309_ADDR              0x78

typedef union
{
    uint8_t page[SSD1309_PAGE_SIZE_BYTES];
    uint8_t rows[8][SSD1309_ROW_SIZE_BYTES];
} UPageRow_t;

typedef union
{
    uint8_t buffer[SSD1309_GDDRAM_SIZE_BYTES];
    UPageRow_t pages[8];
} UFrameBuffer_t;

//=====================================================================================================================
// Globals
//=====================================================================================================================

static UFrameBuffer_t g_framebuffer = { 0 };

static bool    dmaIsEnabled = false;

static bool    dmaInProgress = false;
static bool    nonDMAInProgress = false;

static SI2CTransfer_t g_i2cTransfer = { .address = SSD1309_ADDR };

uint8_t SSD1309_INIT_SEQ[] = {
    SSD1309_DISPLAY_OFF,

    SSD1309_SET_DISPLAY_CLOCK_DIV, 0xA0, // clock divide ratio (0x00=1) and oscillator frequency (0x8)
    SSD1309_SET_START_LINE,
    SSD1309_MEMORY_MODE, 0x00,
    SSD1309_SEG_REMAP_FLIP,
    SSD1309_COM_SCAN_DEC,
    SSD1309_SET_COM_PINS, 0x12,          // alternative com pin config (bit 4), disable left/right remap (bit 5) -> datasheet option 5 with COM_SCAN_INC, 8 with COM_SCAN_DEC
    SSD1309_SET_CONTRAST, 0x6F,
    SSD1309_SET_PRECHARGE, 0xD3,         // precharge period 0x22/F1
    SSD1309_SET_VCOM_DESELECT, 0x20,     // vcomh deselect level

    SSD1309_DEACTIVATE_SCROLL,
    SSD1309_DISPLAY_ALL_ON_RESUME,       // normal mode: ram -> display
    SSD1309_NORMAL_DISPLAY,              // non-inverted mode

    // only do this once: we send the entire framebuffer every time using DMA
    SSD1309_COLUMN_START_ADDRESS_LOW_NIBBLE | 0x00,
    SSD1309_COLUMN_START_ADDRESS_HI_NIBBLE  | 0x00,
    SSD1309_SET_PAGE_START_ADDRESS          | 0x00,

    SSD1309_DISPLAY_ON
};

uint8_t SSD1309_FLIP0_SEQ[] = {
    SSD1309_SEG_REMAP_FLIP,
    SSD1309_COM_SCAN_DEC
};

uint8_t SSD1309_FLIP1_SEQ[] = {
    SSD1309_SEG_REMAP_NORMAL,
    SSD1309_COM_SCAN_INC
};

// FOR DATASHEET MODE 5:
// page 0-3 are the even lines, page 4-7 are the odd lines
// every page has 8 COM lines, so 64 lines in total (for a height of 64 pixels)
// this also applies for mode 8, but in reverse

// page 0 (COM 0-7)   == ROW 0-2-4-6-8-10-12-14         (0-127) (128 bytes, 16 per row)
// page 1 (COM 8-15)  == ROW 16-18-20-22-24-26-28-30    (128-255)
// page 2 (COM 16-23) == ROW 32-34-36-38-40-42-44-46    (256-383)
// page 3 (COM 24-31) == ROW 48-50-52-54-56-58-60-62    (384-511)

// page 4 (COM 32-39) == ROW 1-3-5-7-9-11-13-15         (512-639)
// page 5 (COM 40-47) == ROW 17-19-21-23-25-27-29-31    (640-767)
// page 6 (COM 48-55) == ROW 33-35-37-39-41-43-45-47    (768-895)
// page 7 (COM 56-63) == ROW 49-51-53-55-57-59-61-63    (896-1023)

//=====================================================================================================================
// Static protos
//=====================================================================================================================

static void dispConfigureDMA(void);
static void dispToggleDMA(bool);

static void dispWriteCommand(uint8_t);
static void dispWriteFbuf(void);
static void dispSyncFramebuffer(void);

static void dispWriteMulti(uint8_t *, size_t);
static void dispI2CDMACallback(bool);
static void dispI2RegularCallback(bool);

//=====================================================================================================================
// Functions
//=====================================================================================================================

void initDisplay(void)
{
    i2cRegisterCallback(dispI2RegularCallback);

    i2cInitDisplayDMA(dispI2CDMACallback);

    for (size_t i = 0; i < sizeof(SSD1309_INIT_SEQ) / sizeof(SSD1309_INIT_SEQ[0]); i++)
    {
        dispWriteCommand(SSD1309_INIT_SEQ[i]);
    }

    for (size_t i = 0; i < SSD1309_HEIGHT; i += 4)
    {
        for (size_t j = 0; j < SSD1309_WIDTH; j += 4)
        {
            dispDrawPixel(j, i);
        }
    }

    dispToggleDMA(true);

    dispSyncFramebuffer();

    while(dmaInProgress) {}

    while(true);
}

/**
 * @brief draw a pixel in the display.
 * 
 * offsets are based on 1,1. calculations are performed inside to ensure page alignment.
 * 
 * @param col column to draw in (x)
 * @param row row to draw in (y)
 */
void dispDrawPixel(uint8_t col, uint8_t row)
{
    // rsh4 gives us the correct page from 0-3
    size_t page = row >> 4;
    size_t offset = (page << 4) + 1; // either 0, 16, 32, 48: sub this from (row >> 1)
    size_t uneven = 0;

    // coordinates start at 1, but calculation at 0
    if (((row - 1) % 2) == 0)
    {
        // when uneven, start at page 4.
        page += 4;
        uneven = 1;
        offset -= 1; // don't sub the offset for uneven numbers
    }

    // if we multiply this rsh value by 16 we get the offset (0, 16, 32, 48)
    // if we subtract this from the original y position and rsh by 1 we get the right row to work on
    // for example, for (17, 13) we need to work on row 6 in page 0. div 2, round up
    // and for (17, 36) we need to work on row 1 in page 6. sub 32, end up with 4 (y=3), div 2, do not round
    
    // (17, 13)                          0            13 -  1       >> 1  - 0       ( == 6)
    // (17, 36)                          6            36 -  32      >> 1  - 1       ( == 1)
    // (17, 62)                          7            62 -  48      >> 1  - 1       ( == 6)
    // (17, 63)                          3            63 -  49      >> 1  - 0       ( == 7)
    // (17, 64)                          7            64 -  48      >> 1  - 1       ( == 7)
    uint8_t *pRow = &g_framebuffer.pages[page].rows[((row - offset) >> 1) - uneven];

    // now that we have the row we just need to figure out the Y in the column where the pixel will go
    // every column consists of 8 bytes, so for example a pixel in column 7 should go into byte 0, bit 6
    // another example, column 18 is byte 2, bit 1
    // first we have to rsh3 the column number to get the correct offset in the row, then set the bit for the correct number
    // to find the correct bit we modulo by 8, minus one
    // (17, 13) would need to end up in byte 2, bit 1 TODO: verify endianness

    //                2        |=  7F (01111111)
    *(pRow + ((col - 1) >> 3)) |= (1 << ((col % 8) - 1));
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
    g_i2cTransfer.pBuffer = g_framebuffer.buffer;
    g_i2cTransfer.len = 1024;

    i2cTransferDisplayDMA(&g_i2cTransfer);
}

static void dispWriteFbuf(void)
{
    nonDMAInProgress = true;
    g_i2cTransfer.pBuffer = g_framebuffer.buffer;
    g_i2cTransfer.len = 1024;

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