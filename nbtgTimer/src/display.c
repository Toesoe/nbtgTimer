/**
 * @file display.c
 *
 * @brief display routines for nbtgTimer
 * SSD1309 using DMA/I2C
 * 
 * using horizontal addressing and DMA we'll have a hell of a refresh rate
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "display.h"

#include <stm32g0xx_ll_system.h>
#include <string.h>

#include "board.h"
#include "images.h"


//=====================================================================================================================
// Defines
//=====================================================================================================================

#define SSD1309_WIDTH             128
#define SSD1309_HEIGHT            64

#define SSD1309_PAGE_SIZE_BYTES   128
#define SSD1309_ROW_SIZE_BYTES    16
#define SSD1309_GDDRAM_SIZE_BYTES 1024

#define SSD1309_I2C_ADDR          0x78

//=====================================================================================================================
// Types
//=====================================================================================================================

typedef union
{
    uint8_t page[SSD1309_PAGE_SIZE_BYTES];
    uint8_t rows[8][SSD1309_ROW_SIZE_BYTES];
} __attribute__((packed)) UPageRow_t;

typedef union
{
    uint8_t    buffer[SSD1309_GDDRAM_SIZE_BYTES];
    UPageRow_t pages[8];
} __attribute__((packed)) UFrameBuffer_t;

typedef struct
{
    EDisplayMode_t mode;
    bool           DMAIsEnabled;
    bool           DMAInProgress;
    bool           nonDMAInProgress;
} SDisplayControl_t;

typedef struct
{
    uint8_t command;
    uint8_t parameter;
    bool    hasParameter;
} SDisplayCommand_t;

//=====================================================================================================================
// Globals
//=====================================================================================================================

static __attribute__((aligned(4))) UFrameBuffer_t g_framebuffer      = { 0 };
static SI2CTransfer_t                             g_i2cTransfer      = { .address = SSD1309_I2C_ADDR };
static SSPITransfer_t                             g_spiTransfer      = { 0 };

static SDisplayControl_t                          g_displayState     = { 0 };

SDisplayCommand_t        SSD1309_INIT_SEQ[] = {
    (SDisplayCommand_t){ SSD1309_DISPLAY_OFF,                            0x00, false },

    (SDisplayCommand_t){ SSD1309_SET_DISPLAY_CLOCK_DIV,                  0x80, true  }, // clock divide ratio (0x00=1) and oscillator frequency (0x8)
    (SDisplayCommand_t){ SSD1309_SET_DISPLAY_OFFSET,                     0x00, true  },
    (SDisplayCommand_t){ SSD1309_SET_START_LINE,                         0x00, false },
    (SDisplayCommand_t){ SSD1309_CHARGE_PUMP,                            0x14, true  },
    (SDisplayCommand_t){ SSD1309_MEMORY_MODE,                            0x00, true  },
    (SDisplayCommand_t){ SSD1309_SEG_REMAP_FLIP,                         0x00, false },
    (SDisplayCommand_t){ SSD1309_COM_SCAN_DEC,                           0x00, false },
    (SDisplayCommand_t){ SSD1309_SET_COM_PINS,                           0x12, true  }, // alternative com pin config (bit 4), disable left/right remap (bit 5) -> datasheet
    // option 5 with COM_SCAN_INC, 8 with COM_SCAN_DEC
    (SDisplayCommand_t){ SSD1309_SET_CONTRAST,                           0x6F, true  },
    (SDisplayCommand_t){ SSD1309_SET_PRECHARGE,                          0x22, true  }, // precharge period 0x22/F1
    (SDisplayCommand_t){ SSD1309_SET_VCOM_DESELECT,                      0x40, true  }, // vcomh deselect level

    (SDisplayCommand_t){ SSD1309_DEACTIVATE_SCROLL,                      0x00, false },
    (SDisplayCommand_t){ SSD1309_DISPLAY_ALL_ON_RESUME,                  0x00, false }, // normal mode: ram -> display
    (SDisplayCommand_t){ SSD1309_NORMAL_DISPLAY,                         0x00, false }, // non-inverted mode

    // only do this once: we send the entire framebuffer every time using DMA
    (SDisplayCommand_t){ SSD1309_COLUMN_START_ADDRESS_LOW_NIBBLE | 0x00, 0x00, false },
    (SDisplayCommand_t){ SSD1309_COLUMN_START_ADDRESS_HI_NIBBLE | 0x00,  0x00, false },
    (SDisplayCommand_t){ SSD1309_SET_PAGE_START_ADDRESS | 0x00,          0x00, false },

    (SDisplayCommand_t){ SSD1309_DISPLAY_ON,                             0x00, false },
};

SDisplayCommand_t SSD1309_FLIP0_SEQ[] = {
    (SDisplayCommand_t){ SSD1309_SEG_REMAP_FLIP, 0x00, false },
    (SDisplayCommand_t){ SSD1309_COM_SCAN_DEC,   0x00, false },
};

SDisplayCommand_t SSD1309_FLIP1_SEQ[] = {
    (SDisplayCommand_t){ SSD1309_SEG_REMAP_NORMAL, 0x00, false },
    (SDisplayCommand_t){ SSD1309_COM_SCAN_INC,     0x00, false },
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

static void dispToggleDMA(bool);

static void dispWriteCommand(SDisplayCommand_t);
static void dispWriteFbuf(void);
static void dispSyncFramebuffer(void);

static void dispWriteMulti(uint8_t *, size_t);
static void dispDMACallback(bool);
static void dispRegularCallback(bool);

//=====================================================================================================================
// Functions
//=====================================================================================================================

void initDisplay(EDisplayMode_t displayMode)
{
    g_displayState.mode             = displayMode;
    g_displayState.DMAInProgress    = false;
    g_displayState.DMAIsEnabled     = false;
    g_displayState.nonDMAInProgress = false;

    if (displayMode == MODE_I2C)
    {
        i2cRegisterCallback(dispRegularCallback);
        i2cInitDisplayDMA(dispDMACallback);
    }
    else
    {
        // SPI stuff
        spiInitDisplayDMA(dispDMACallback);
        resetDisplay(true);
        hwDelayMs(10);
        resetDisplay(false);
        hwDelayMs(500);
    }

    for (size_t i = 0; i < sizeof(SSD1309_INIT_SEQ) / sizeof(SSD1309_INIT_SEQ[0]); i++)
    {
        dispWriteCommand(SSD1309_INIT_SEQ[i]);
    }
}

/**
 * @brief draw a pixel in the display buffer
 *
 * offsets are based on 1,1. calculations are performed inside to ensure page alignment.
 *
 * @param col column to draw in (x)
 * @param row row to draw in (y)
 */
void dispDrawPixel(uint8_t col, uint8_t row)
{
    // 0-based indexing
    col--;
    row--;

    // determine which page contains this row
    uint8_t page, bitPosition;

    if (row % 2 == 0)
    {
        // even rows (0,2,4,...) - go to pages 0-3
        page = row / 16;
        // for even rows, calculate bit position within the byte
        // row 0 → bit 0, row 2 → bit 1, row 4 → bit 2, etc.
        bitPosition = (row % 16) / 2;
    }
    else
    {
        // odd rows (1,3,5,...) - go to pages 4-7
        page = 4 + (row / 16);
        // For odd rows, calculate bit position within the byte
        // row 1 → bit 0, row 3 → bit 1, row 5 → bit 2, etc.
        bitPosition = (row % 16) / 2;
    }

    // in horizontal addressing mode, each byte represents a single column so the byteOffset is simply the column number
    g_framebuffer.pages[page].page[col] |= (1 << bitPosition);
}

static void dispToggleDMA(bool enable)
{
    g_displayState.DMAIsEnabled = enable;
}

static void dispWriteCommand(SDisplayCommand_t cmd)
{
    uint8_t buf[3] = { 0x00, cmd.command };
    if (cmd.hasParameter)
    {
        buf[2] = cmd.parameter;
    }
    spiSendCommand(buf, (cmd.hasParameter ? 3 : 2));
}

static void dispResetPointers(void)
{
    uint8_t buf[]  = { 0x00, SSD1309_COLUMN_ADDR, 0x00, 0x7F };
    uint8_t buf2[] = { 0x00, SSD1309_PAGE_ADDR, 0x00, 0x07 };

    spiSendCommand(buf, 4);
    spiSendCommand(buf2, 4);
}

/**
 * @brief yeet the framebuffer to the display using DMA
 *
 */
static void dispSyncFramebuffer(void)
{
    if (!g_displayState.DMAIsEnabled) return;

    g_displayState.DMAInProgress = true;

    if (g_displayState.mode == MODE_I2C)
    {
        g_i2cTransfer.pBuffer        = g_framebuffer.buffer;
        g_i2cTransfer.len            = 1024;
        i2cTransferDisplayDMA(&g_i2cTransfer);
    }
    else
    {
        g_spiTransfer.pBuffer = g_framebuffer.buffer;
        g_spiTransfer.len = 1024;
        spiTransferBlockDMA(&g_spiTransfer);
    }
}

static void dispWriteFbuf(void)
{
    g_displayState.nonDMAInProgress = true;
    spiWriteData(g_framebuffer.buffer, 1024);
}

static void dispDMACallback(bool isComplete)
{
    g_displayState.DMAInProgress = false;
}

static void dispRegularCallback(bool isComplete)
{
    g_displayState.nonDMAInProgress = false;
}