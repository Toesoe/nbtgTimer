/**
 * @file display.c
 *
 * @brief display routines for nbtgTimer
 *
 * using horizontal addressing and DMA we get about 60fps at 8MHz: 30 is plenty so we use a hw timer on autoreload
 * built for SSD1309 displays in Mode 5
 *
 * graphical display parts pulled from https://github.com/DuyTrandeLion/nrf52-ssd1309, which is licensed under MIT:
 *
 *  MIT License
 *
 * Copyright (c) 2021 Duy Lion Tran
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "display.h"

#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <stm32g0xx_ll_system.h>
#include <string.h>

#include "board.h"

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

#define DEG_TO_RAD(a)             (a * 3.14 / 180.0)

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
    __attribute__((aligned(4))) UFrameBuffer_t buffer1;
    __attribute__((aligned(4))) UFrameBuffer_t buffer2;
} SFrameBuffer_t;

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
    bool                 isEnabled;
    EDisplayMode_t       mode;
    SDMATranferContext_t dmaTransferContext;
    bool                 DMAIsEnabled;
    bool                 DMAInProgress;

    UFrameBuffer_t      *pFrontBuffer;
    UFrameBuffer_t      *pBackBuffer;
    uint8_t              currentX;
    uint8_t              currentY;
} SDisplayContext_t;

//=====================================================================================================================
// Globals
//=====================================================================================================================

static SDisplayContext_t g_displayContext = { 0 };

SFrameBuffer_t           g_framebuffers;

SPixel_t                 g_pxl = {
    (SPoint_t){ 1, 1 },
    COLOR_WHITE
};

SPixel_t                 g_pxl_b = {
    (SPoint_t){ 1, 1 },
    COLOR_BLACK
};

SDisplayCommand_t SSD1309_INIT_SEQ[] = {
    (SDisplayCommand_t){ SSD1309_DISPLAY_OFF,                            0x00, false },

    (SDisplayCommand_t){ SSD1309_SET_DISPLAY_CLOCK_DIV,                  0xA0,
                        true                                                         }, // clock divide ratio (0x00=1) and oscillator frequency (0x8)
    (SDisplayCommand_t){ SSD1309_SET_MULTIPLEX,                          0x3F, true  },
    (SDisplayCommand_t){ SSD1309_SET_DISPLAY_OFFSET,                     0x00, true  },
    (SDisplayCommand_t){ SSD1309_SET_START_LINE,                         0x00, false },
    (SDisplayCommand_t){ SSD1309_SET_MASTER_CONFIG,                      0x8E, true  },
    //(SDisplayCommand_t){ SSD1309_CHARGE_PUMP,                            0x14, true  },
    (SDisplayCommand_t){ SSD1309_MEMORY_MODE,                            0x00, true  },
    (SDisplayCommand_t){ SSD1309_SEG_REMAP_FLIP,                         0x00, false },
    (SDisplayCommand_t){ SSD1309_COM_SCAN_DEC,                           0x00, false },
    (SDisplayCommand_t){ SSD1309_SET_COM_PINS,                           0x12,
                        true                                                         }, // alternative com pin config (bit 4), disable left/right remap (bit 5) -> datasheet
    // option 5 with COM_SCAN_INC, 8 with COM_SCAN_DEC
    (SDisplayCommand_t){ SSD1309_SET_CONTRAST,                           0x6F, true  },
    (SDisplayCommand_t){ SSD1309_SET_PRECHARGE,                          0xF1, true  }, // precharge period 0end.end.x/F1
    (SDisplayCommand_t){ SSD1309_SET_VCOM_DESELECT,                      0x30, true  }, // vcomh deselect level

    (SDisplayCommand_t){ SSD1309_DEACTIVATE_SCROLL,                      0x00, false },
    (SDisplayCommand_t){ SSD1309_DISPLAY_ALL_ON_RESUME,                  0x00, false }, // normal mode: ram -> display
    (SDisplayCommand_t){ SSD1309_NORMAL_DISPLAY,                         0x00, false }, // non-inverted mode

    // only do this once: we send the entire framebuffer every time using DMA
    // only valid for page addressing mode
    //(SDisplayCommand_t){ SSD1309_COLUMN_START_ADDRESS_LOW_NIBBLE | 0x00, 0x00, false },
    //(SDisplayCommand_t){ SSD1309_COLUMN_START_ADDRESS_HI_NIBBLE | 0x00,  0x00, false },
    //(SDisplayCommand_t){ SSD1309_SET_PAGE_START_ADDRESS | 0x00,          0x00, false },
};

static const SDisplayCommand_t SSD1309_FLIP0_SEQ[] = {
    // default mode
    (SDisplayCommand_t){ SSD1309_SEG_REMAP_FLIP, 0x00, false },
    (SDisplayCommand_t){ SSD1309_COM_SCAN_DEC,   0x00, false },
};

static const SDisplayCommand_t SSD1309_FLIP1_SEQ[] = {
    (SDisplayCommand_t){ SSD1309_SEG_REMAP_NORMAL, 0x00, false },
    (SDisplayCommand_t){ SSD1309_COM_SCAN_INC,     0x00, false },
};

//=====================================================================================================================
// Static protos
//=====================================================================================================================

static uint16_t dispNormalizeTo0_360(uint16_t);

static void     dispWriteCommand(SDisplayCommand_t);
static void     dispSyncFramebuffer(void);

static void     dispDMACallback(bool);
static void     dispRegularCallback(bool);

static void     testPage(uint8_t);

//=====================================================================================================================
// Functions
//=====================================================================================================================

void initDisplay(EDisplayMode_t displayMode)
{
    g_displayContext.mode          = displayMode;
    g_displayContext.DMAInProgress = false;
    g_displayContext.DMAIsEnabled  = false;
    g_displayContext.isEnabled     = false;

    memset(&g_framebuffers.buffer1.buffer, 0, SSD1309_GDDRAM_SIZE_BYTES);
    memset(&g_framebuffers.buffer2.buffer, 0, SSD1309_GDDRAM_SIZE_BYTES);

    g_displayContext.pFrontBuffer = &g_framebuffers.buffer1;
    g_displayContext.pBackBuffer  = &g_framebuffers.buffer2;

    memset(&g_displayContext.dmaTransferContext, 0, sizeof(SDMATranferContext_t));

    g_displayContext.dmaTransferContext.address     = SSD1309_I2C_ADDR;
    g_displayContext.dmaTransferContext.pBuffer     = g_displayContext.pFrontBuffer->buffer;
    g_displayContext.dmaTransferContext.len         = SSD1309_GDDRAM_SIZE_BYTES;
    g_displayContext.dmaTransferContext.transferred = 0;

    if (displayMode == MODE_I2C)
    {
        i2cInitDisplayDMA(dispDMACallback);
    }
    else
    {
        // SPI stuff
        spiInitDisplayDMA(dispDMACallback);
        registerDisplayFramerateTimerCallback(dispSyncFramebuffer);
        resetDisplay(true);
        hwDelayMs(10);
        resetDisplay(false);
        hwDelayMs(500);
    }

    for (size_t i = 0; i < sizeof(SSD1309_INIT_SEQ) / sizeof(SSD1309_INIT_SEQ[0]); i++)
    {
        dispWriteCommand(SSD1309_INIT_SEQ[i]);
    }

    //uint8_t buf[5] = { 0x91, 0x3F, 0x3F, 0x3F, 0x3F };
    //spiSendCommand(buf, 5);

    g_displayContext.DMAIsEnabled = true;
    g_displayContext.isEnabled = true;
    hwDelayMs(100);

    dispWriteCommand((SDisplayCommand_t){ SSD1309_DISPLAY_ON, 0x00, false });
}

void toggleDisplay(bool enable)
{
    if (enable)
    {
        dispWriteCommand((SDisplayCommand_t){ SSD1309_DISPLAY_ON, 0x00, false });
        g_displayContext.isEnabled = true;
    }
    else
    {
        dispWriteCommand((SDisplayCommand_t){ SSD1309_DISPLAY_OFF, 0x00, false });
        g_displayContext.isEnabled = false;
    }
}

/**
 * @brief draw a pixel in the display buffer
 *
 * offsets are based on 1,1. calculations are performed inside to ensure page alignment.
 *
 */
void dispDrawPixel(SPixel_t pixel)
{
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

    if ((pixel.coordinates.x > SSD1309_WIDTH) || pixel.coordinates.y > SSD1309_HEIGHT) return;

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
        g_displayContext.pFrontBuffer->pages[page].page[x] |= (1 << bitPosition);
    }
    else
    {
        g_displayContext.pFrontBuffer->pages[page].page[x] &= ~(1 << bitPosition);
    }
}

/**
 * @brief draw a line with Bresenham's algo
 *
 * @param line line to draw
 */
void dispDrawLine(SLine_t line)
{
    if ((line.start.x < 1 || line.start.x > SSD1309_WIDTH) || (line.end.x < 1 || line.end.x > SSD1309_WIDTH) ||
        (line.start.y < 1 || line.start.y > SSD1309_HEIGHT) || (line.end.y < 1 || line.end.y > SSD1309_HEIGHT))
    {
        return;
    }

    int16_t x0  = line.start.x - 1; // Adjust for 1-based addressing
    int16_t y0  = line.start.y - 1;
    int16_t x1  = line.end.x - 1;
    int16_t y1  = line.end.y - 1;

    int16_t dx  = abs(x1 - x0);
    int16_t sx  = (x0 < x1) ? 1 : -1;
    int16_t dy  = abs(y1 - y0);
    int16_t sy  = (y0 < y1) ? 1 : -1;
    int16_t err = (dx > dy ? dx : -dy) / 2;
    int16_t e2;

    while (1)
    {
        // DEBUG: Print coordinates for tracing
        // printf("Drawing at: (%d, %d)\n", x0 + 1, y0 + 1);

        dispDrawPixel((SPixel_t){
        { x0 + 1, y0 + 1 },
        COLOR_WHITE
        });

        if (x0 == x1 && y0 == y1) break;

        e2 = err;
        if (e2 > -dx)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dy)
        {
            err += dx;
            y0 += sy;
        }
    }
}

char dispWriteChar(char ch, FontDef Font, EColor_t color)
{
    uint32_t i, b, j;

    /* Check if character is valid */
    if (ch < 32 || ch > 126)
    {
        return 0;
    }

    /* Check remaining space on current line */
    if ((SSD1309_WIDTH < (g_displayContext.currentX + Font.FontWidth)) ||
        (SSD1309_HEIGHT < (g_displayContext.currentY + Font.FontHeight)))
    {
        /* Not enough space on current line */
        return 0;
    }

    /* Use the font to write */
    for (i = 0; i < Font.FontHeight; i++)
    {
        b = Font.data[(ch - 32) * Font.FontHeight + i];

        for (j = 0; j < Font.FontWidth; j++)
        {
            if ((b << j) & 0x8000)
            {
                dispDrawPixel((SPixel_t){
                { g_displayContext.currentX + j, (g_displayContext.currentY + i) },
                color
                });
            }
            else
            {
                dispDrawPixel((SPixel_t){
                { g_displayContext.currentX + j, (g_displayContext.currentY + i) },
                !color
                });
            }
        }
    }

    /* The current space is now taken */
    g_displayContext.currentX += Font.FontWidth;

    return ch;
}

char dispWriteString(const char *str, FontDef Font, EColor_t color)
{
    /* Write until null-byte */
    while (*str)
    {
        if (dispWriteChar(*str, Font, color) != *str)
        {
            /* Char could not be written */
            return *str;
        }

        /* Next char */
        str++;
    }

    /* Everything ok */
    return *str;
}

void dispSetCursor(uint8_t x, uint8_t y)
{
    g_displayContext.currentX = x;
    g_displayContext.currentY = y;
}

void dispDrawArc(SPoint_t center, uint8_t radius, uint16_t start_angle, uint16_t sweep, EColor_t color)
{
#define CIRCLE_APPROXIMATION_SEGMENTS 36
    float    approx_degree;
    uint32_t approx_segments;
    uint8_t  xp1, xp2;
    uint8_t  yp1, yp2;
    uint16_t loc_sweep       = 0;
    uint16_t loc_angle_count = 0;
    uint32_t count           = 0;

    float    rad;

    loc_sweep       = dispNormalizeTo0_360(sweep);
    loc_angle_count = dispNormalizeTo0_360(start_angle);

    count           = (loc_angle_count * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_segments = (loc_sweep * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_degree   = loc_sweep / (float)approx_segments;

    while (count < approx_segments)
    {
        xp1 = center.x + (int8_t)(sin(rad) * radius);
        yp1 = center.y + (int8_t)(cos(rad) * radius);
        count++;
#if 1
        if (count != approx_segments)
        {
            rad = DEG_TO_RAD(count * approx_degree);
        }
        else
        {
            rad = DEG_TO_RAD(loc_sweep);
        }
#endif
        xp2 = center.x + (int8_t)(sin(rad) * radius);
        yp2 = center.y + (int8_t)(cos(rad) * radius);
        dispDrawLine((SLine_t){
        { xp1, yp1 },
        { xp2, yp2 },
        color
        });
    }
}

void dispDrawCircle(SPoint_t center, uint8_t radius, EColor_t color, bool fill)
{
    int32_t x   = -radius;
    int32_t y   = 0;
    int32_t err = 2 - 2 * radius;
    int32_t e2;

    if (center.x > SSD1309_WIDTH || center.y > SSD1309_HEIGHT)
    {
        return;
    }

    do
    {
        if (!fill)
        {
            dispDrawPixel((SPixel_t){
            { center.x - x, center.y + y },
            color
            });
            dispDrawPixel((SPixel_t){
            { center.x + x, center.y + y },
            color
            });
            dispDrawPixel((SPixel_t){
            { center.x + x, center.y - y },
            color
            });
            dispDrawPixel((SPixel_t){
            { center.x - x, center.y - y },
            color
            });
        }
        else
        {
            for (uint8_t _y = (center.y + y); _y >= (center.y - y); _y--)
            {
                for (uint8_t _x = (center.x - x); _x >= (center.x + x); _x--)
                {
                    dispDrawPixel((SPixel_t){
                    { _x, _y },
                    color
                    });
                }
            }
        }
        e2 = err;

        if (e2 <= y)
        {
            y++;
            err = err + (y * 2 + 1);

            if (-x == y && e2 <= x)
            {
                e2 = 0;
            }
            else
            {
                /* Nothing to do */
            }
        }
        else
        {
            /* Nothing to do */
        }

        if (e2 > x)
        {
            x++;
            err = err + (x * 2 + 1);
        }
        else
        {
            /* Nothing to do */
        }
    }
    while (x <= 0);

    return;
}

void dispDrawPolyline(const SPoint_t *par_vertex, uint16_t par_size, EColor_t color)
{
    uint16_t i;

    if (par_vertex != NULL)
    {
        for (i = 1; i < par_size; i++)
        {
            dispDrawLine((SLine_t){
            { par_vertex[i - 1].x, par_vertex[i - 1].y },
            { par_vertex[i].x,     par_vertex[i].y     },
            color
            });
        }
    }
    else
    {
        /* Nothing to do */
    }
    return;
}

void dispDrawRectangle(SPoint_t start, SPoint_t end, EColor_t color)
{
    dispDrawLine((SLine_t){
    { start.x, start.y },
    { end.x,   start.y },
    color
    });
    dispDrawLine((SLine_t){
    { end.x, start.y },
    { end.x, end.y   },
    color
    });
    dispDrawLine((SLine_t){
    { end.x,   end.y },
    { start.x, end.y },
    color
    });
    dispDrawLine((SLine_t){
    { start.x, end.y   },
    { start.x, start.y },
    color
    });
}

void dispDrawFilledRectangle(SPoint_t start, SPoint_t end, EColor_t color)
{
    uint8_t x_start = ((start.x <= end.x) ? start.x : end.x);
    uint8_t x_end   = ((start.x <= end.x) ? end.x : start.x);
    uint8_t y_start = ((start.y <= end.y) ? start.y : end.y);
    uint8_t y_end   = ((start.y <= end.y) ? end.y : start.y);

    for (uint8_t y = y_start; (y <= y_end) && (y < SSD1309_HEIGHT); y++)
    {
        for (uint8_t x = x_start; (x <= x_end) && (x < SSD1309_WIDTH); x++)
        {
            dispDrawPixel((SPixel_t){
            { x, y },
            color
            });
        }
    }
    return;
}

void dispDrawBitmap(SPoint_t coords, const unsigned char *bitmap, uint8_t w, uint8_t h, EColor_t color)
{
    uint8_t byte      = 0;
    int16_t byteWidth = (w + 7) / 8; /* Bitmap scanline pad = whole byte */

    uint8_t x         = coords.x;
    uint8_t y         = coords.y;

    if (coords.x > SSD1309_WIDTH || coords.y > SSD1309_HEIGHT)
    {
        return;
    }

    for (uint8_t j = 0; j < h; j++, y++)
    {
        for (uint8_t i = 0; i < w; i++)
        {
            if (i & 7)
            {
                byte <<= 1;
            }
            else
            {
                byte = (*(const unsigned char *)(&bitmap[j * byteWidth + i / 8]));
            }

            if (byte & 0x80)
            {
                dispDrawPixel((SPixel_t){
                { x + i, y },
                color
                });
            }
        }
    }

    return;
}

static uint16_t dispNormalizeTo0_360(uint16_t par_deg)
{
    uint16_t loc_angle;
    if (par_deg <= 360)
    {
        loc_angle = par_deg;
    }
    else
    {
        loc_angle = par_deg % 360;
        loc_angle = ((par_deg != 0) ? par_deg : 360);
    }
    return loc_angle;
}

static void testPage(uint8_t page)
{
    page = 7 - page;

    memset(g_displayContext.pFrontBuffer->buffer, 0, SSD1309_GDDRAM_SIZE_BYTES);
    // Fill an entire page with all pixels on
    for (int i = 0; i < SSD1309_PAGE_SIZE_BYTES; i++)
    {
        g_displayContext.pFrontBuffer->pages[page].page[i] = 0xFF;
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

 TODO: add quick checksum to check if buffers need updating at all
 *
 */
static void dispSyncFramebuffer(void)
{
    if (!g_displayContext.isEnabled || !g_displayContext.DMAIsEnabled || g_displayContext.DMAInProgress) return;

    g_displayContext.DMAInProgress                  = true;
    g_displayContext.dmaTransferContext.transferred = 0;

    // dispDrawPixel(g_pxl);

    // g_pxl.coordinates.x++;

    // if (g_pxl.coordinates.x == SSD1309_WIDTH + 1)
    // {
        // g_pxl.coordinates.y++;
        // g_pxl.coordinates.x = 1;
    // }

    // if (g_pxl.coordinates.y == SSD1309_HEIGHT + 1)
    // {
        // g_pxl.coordinates.x = 0;
        // g_pxl.coordinates.y = 0;
    // }

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
    // swap buffers for vsync
    memcpy(g_displayContext.pBackBuffer->buffer, g_displayContext.pFrontBuffer->buffer, SSD1309_GDDRAM_SIZE_BYTES);
    UFrameBuffer_t *pTmp                        = g_displayContext.pFrontBuffer;
    g_displayContext.pFrontBuffer               = g_displayContext.pBackBuffer;
    g_displayContext.pBackBuffer                = pTmp;
    g_displayContext.dmaTransferContext.pBuffer = g_displayContext.pFrontBuffer->buffer;
    g_displayContext.DMAInProgress              = false;
}