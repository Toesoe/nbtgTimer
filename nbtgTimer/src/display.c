/**
 * @file display.c
 *
 * @brief display routines for nbtgTimer
 *
 * using horizontal addressing and DMA we get about 60fps at 8MHz
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
#define SSD1309_NUM_PAGES         8

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
    uint8_t command;
    uint8_t parameter;
    bool    hasParameter;
} SDisplayCommand_t;

typedef struct
{
    uint8_t  address; // not used for SPI but used for bitcompat with SI2CTransfer_t
    uint8_t *pBuffer;
    size_t   len;
    size_t   transferred;
} SDMATranferContext_t;

typedef struct
{
    EDisplayMode_t                             mode;
    bool                                       DMAIsEnabled;
    bool                                       DMAInProgress;
    bool                                       nonDMAInProgress;

    __attribute__((aligned(4))) UFrameBuffer_t framebuffer;
    SDMATranferContext_t                       dmaTransferContext;
} SDisplayContext_t;

//=====================================================================================================================
// Globals
//=====================================================================================================================

static SDisplayContext_t       g_displayContext   = { 0 };

SDisplayCommand_t        SSD1309_INIT_SEQ[] = {
    (SDisplayCommand_t){ SSD1309_DISPLAY_OFF,                            0x00, false },

    (SDisplayCommand_t){ SSD1309_SET_DISPLAY_CLOCK_DIV,                  0xA0, true  }, // clock divide ratio (0x00=1) and oscillator frequency (0x8)
    (SDisplayCommand_t){ SSD1309_SET_MULTIPLEX,                          0x3F, true  },
    (SDisplayCommand_t){ SSD1309_SET_DISPLAY_OFFSET,                     0x00, true  },
    (SDisplayCommand_t){ SSD1309_SET_START_LINE,                         0x00, false },
    (SDisplayCommand_t){ SSD1309_SET_MASTER_CONFIG,                      0x8E, true  },
    //(SDisplayCommand_t){ SSD1309_CHARGE_PUMP,                            0x14, true  },
    (SDisplayCommand_t){ SSD1309_MEMORY_MODE,                            0x00, true  },
    (SDisplayCommand_t){ SSD1309_SEG_REMAP_FLIP,                         0x00, false },
    (SDisplayCommand_t){ SSD1309_COM_SCAN_DEC,                           0x00, false },
    (SDisplayCommand_t){ SSD1309_SET_COM_PINS,                           0x12, true  }, // alternative com pin config (bit 4), disable left/right remap (bit 5) -> datasheet
    // option 5 with COM_SCAN_INC, 8 with COM_SCAN_DEC
    (SDisplayCommand_t){ SSD1309_SET_CONTRAST,                           0x6F, true  },
    (SDisplayCommand_t){ SSD1309_SET_PRECHARGE,                          0xF1, true  }, // precharge period 0x22/F1
    (SDisplayCommand_t){ SSD1309_SET_VCOM_DESELECT,                      0x30, true  }, // vcomh deselect level

    (SDisplayCommand_t){ SSD1309_DEACTIVATE_SCROLL,                      0x00, false },
    (SDisplayCommand_t){ SSD1309_DISPLAY_ALL_ON_RESUME,                  0x00, false }, // normal mode: ram -> display
    (SDisplayCommand_t){ SSD1309_NORMAL_DISPLAY,                         0x00, false }, // non-inverted mode

    // only do this once: we send the entire framebuffer every time using DMA
    (SDisplayCommand_t){ SSD1309_COLUMN_START_ADDRESS_LOW_NIBBLE | 0x00, 0x00, false },
    (SDisplayCommand_t){ SSD1309_COLUMN_START_ADDRESS_HI_NIBBLE | 0x00,  0x00, false },
    (SDisplayCommand_t){ SSD1309_SET_PAGE_START_ADDRESS | 0x00,          0x00, false },
};

static const SDisplayCommand_t SSD1309_FLIP0_SEQ[] = {
    (SDisplayCommand_t){ SSD1309_SEG_REMAP_FLIP, 0x00, false },
    (SDisplayCommand_t){ SSD1309_COM_SCAN_DEC,   0x00, false },
};

static const SDisplayCommand_t SSD1309_FLIP1_SEQ[] = {
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

static void dispWriteCommand(SDisplayCommand_t);
static void dispSyncFramebuffer(void);

static void dispDMACallback(bool);
static void dispRegularCallback(bool);

static void testPage(uint8_t);

//=====================================================================================================================
// Functions
//=====================================================================================================================

void initDisplay(EDisplayMode_t displayMode)
{
    g_displayContext.mode             = displayMode;
    g_displayContext.DMAInProgress    = false;
    g_displayContext.DMAIsEnabled     = false;
    g_displayContext.nonDMAInProgress = false;

    memset(g_displayContext.framebuffer.buffer, 0, 1024);
    memset(&g_displayContext.dmaTransferContext, 0, sizeof(SDMATranferContext_t));

    g_displayContext.dmaTransferContext.address = SSD1309_I2C_ADDR;
    g_displayContext.dmaTransferContext.pBuffer = g_displayContext.framebuffer.buffer;
    g_displayContext.dmaTransferContext.len     = SSD1309_GDDRAM_SIZE_BYTES;
    g_displayContext.dmaTransferContext.transferred = 0;

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

    uint8_t buf[5] = { 0x91, 0x3F, 0x3F, 0x3F, 0x3F };
    spiSendCommand(buf, 5);

    dispWriteCommand((SDisplayCommand_t){ SSD1309_DISPLAY_ON, 0x00, false });
    hwDelayMs(100);
    g_displayContext.DMAIsEnabled = true;

    dispSyncFramebuffer(); // zero out GDDRAM

    while (g_displayContext.DMAInProgress) {}
}

/**
 * @brief draw a pixel in the display buffer
 *
 * offsets are based on 1,1. calculations are performed inside to ensure page alignment.
 *
 */
void dispDrawPixel(SPixel_t pixel)
{
    if ((pixel.coordinates.x < 1 || pixel.coordinates.x > SSD1309_WIDTH) || (pixel.coordinates.y < 1 || pixel.coordinates.y > SSD1309_HEIGHT)) return;

    // Convert to 0-based coordinates
    uint8_t x = pixel.coordinates.x - 1;
    uint8_t y = pixel.coordinates.y - 1;

    // Compute the correct page (row-to-page mapping)
    uint8_t page = y / 8;

    // Compute the correct bit position
    uint8_t bitPosition = y % 8;

    // Set the pixel in the framebuffer
    if (pixel.color == COLOR_WHITE)
    {
        g_displayContext.framebuffer.pages[page].page[x] |= (1 << bitPosition);
    }
    else
    {
        g_displayContext.framebuffer.pages[page].page[x] &= ~(1 << bitPosition);
    }
}

/**
 * @brief draw a line with Bresenham's algo
 * 
 * @param line line to draw
 */
void dispDrawLine(SLine_t line)
{
    if ((line.start.x < 1 || line.start.x > SSD1309_WIDTH) || 
        (line.end.x < 1 || line.end.x > SSD1309_WIDTH) ||
        (line.start.y < 1 || line.start.y > SSD1309_HEIGHT) || 
        (line.end.y < 1 || line.end.y > SSD1309_HEIGHT))
    {
        return;
    }

    int16_t x0 = line.start.x - 1;  // Adjust for 1-based addressing
    int16_t y0 = line.start.y - 1;
    int16_t x1 = line.end.x - 1;
    int16_t y1 = line.end.y - 1;

    int16_t dx = abs(x1 - x0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t dy = abs(y1 - y0);
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = (dx > dy ? dx : -dy) / 2;
    int16_t e2;

    while (1) {
        // DEBUG: Print coordinates for tracing
        //printf("Drawing at: (%d, %d)\n", x0 + 1, y0 + 1); 

        dispDrawPixel((SPixel_t){ { x0 + 1, y0 + 1 }, COLOR_WHITE });

        if (x0 == x1 && y0 == y1) 
            break;

        e2 = err;
        if (e2 > -dx) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy) {
            err += dx;
            y0 += sy;
        }
    }
}

static void testPage(uint8_t page)
{
    page = 7 - page;

    memset(g_displayContext.framebuffer.buffer, 0, SSD1309_GDDRAM_SIZE_BYTES);
    // Fill an entire page with all pixels on
    for (int i = 0; i < SSD1309_PAGE_SIZE_BYTES; i++)
    {
        g_displayContext.framebuffer.pages[page].page[i] = 0xFF;
    }
}

static void dispWriteCommand(SDisplayCommand_t cmd)
{
    uint8_t buf[2] = { cmd.command };
    if (cmd.hasParameter)
    {
        buf[1] = cmd.parameter;
    }
    spiSendCommand(buf, (cmd.hasParameter ? 2 : 1));
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
    if (!g_displayContext.DMAIsEnabled) return;

    g_displayContext.DMAInProgress                  = true;
    g_displayContext.dmaTransferContext.transferred = 0;

    if (g_displayContext.mode == MODE_I2C)
    {
        i2cTransferDisplayDMA((SI2CTransfer_t *)&g_displayContext.dmaTransferContext);
    }
    else
    {
        spiTransferBlockDMA((SSPITransfer_t *)&g_displayContext.dmaTransferContext);
    }
}

static void dispDMACallback(bool isComplete)
{
    g_displayContext.DMAInProgress = false;
}

static void dispRegularCallback(bool isComplete)
{
    g_displayContext.nonDMAInProgress = false;
}