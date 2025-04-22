/**
 * @file display.c
 *
 * @brief display routines
 *
 * using horizontal addressing and DMA we get about 60fps at 8MHz: 30 is plenty so we use a hw timer on autoreload
 * built for SSD1309 displays in Mode 5
 *
 * graphics routines are based on https://github.com/DuyTrandeLion/nrf52-ssd1309, which is licensed under MIT: Copyright (c) 2021 Duy Lion Tran
 * improvements in this version:
 * - double buffering
 * - horizontal addressing mode, allows for DMA'ing the full framebuffer in 1 chunk
 * - removed dependency on floating point math, fixed-point implementations
 *
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "display.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <stm32g0xx_ll_system.h>
#include <string.h>

#include "board.h"
#include "timer.h"

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define SSD1309_WIDTH                 128
#define SSD1309_HEIGHT                64

#define SSD1309_PAGE_SIZE_BYTES       128
#define SSD1309_ROW_SIZE_BYTES        16
#define SSD1309_GDDRAM_SIZE_BYTES     1024
#define SSD1309_NUM_PAGES             8

#define SSD1309_I2C_ADDR              0x78

#define CIRCLE_APPROXIMATION_SEGMENTS 36   // this gives a segment every 10 degrees
#define FIXED_POINT_MATH_SCALE        1024 // 2^10 fixed point scale

#define DEG_TO_RAD(a)                 (a * 3.14 / 180.0)

//=====================================================================================================================
// Constants
//=====================================================================================================================

/** @brief precomputed cosine table, cos(i * 5 deg) * SCALE */
static const int16_t g_cosLUT[72] = { 1024,  1020,  1008,  989,  962,  928,  887,  839,  784,  724,  658,   587,
                                      512,   433,   350,   265,  178,  89,   0,    -89,  -178, -265, -350,  -433,
                                      -512,  -587,  -658,  -724, -784, -839, -887, -928, -962, -989, -1008, -1020,
                                      -1024, -1020, -1008, -989, -962, -928, -887, -839, -784, -724, -658,  -587,
                                      -512,  -433,  -350,  -265, -178, -89,  0,    89,   178,  265,  350,   433,
                                      512,   587,   658,   724,  784,  839,  887,  928,  962,  989,  1008,  1020 };

//=====================================================================================================================
// Types
//=====================================================================================================================

/** @brief page/row union type. allows accessing pages in full or per-row */
typedef union
{
    uint8_t page[SSD1309_PAGE_SIZE_BYTES];
    uint8_t rows[8][SSD1309_ROW_SIZE_BYTES];
} __attribute__((packed)) UPageRow_t;

/** @brief framebuffer union type. full buffer consists of 8 pages. */
typedef union
{
    uint8_t    buffer[SSD1309_GDDRAM_SIZE_BYTES];
    UPageRow_t pages[8];
} __attribute__((packed)) UFrameBuffer_t;

/** @brief main framebuffer struct. this implementation is double-buffered so we have 2 buffers of 1K */
typedef struct
{
    __attribute__((aligned(4))) UFrameBuffer_t buffer1;
    __attribute__((aligned(4))) UFrameBuffer_t buffer2;
} SFrameBuffer_t;

/** @brief display command struct. allows passing in optional parameters */
typedef struct
{
    uint8_t command;
    uint8_t parameter;
    bool    hasParameter;
} SDisplayCommand_t;

/** @brief DMA transfer context */
typedef struct
{
    uint8_t  address; // not used for SPI but used for bitcompat with SI2CTransfer_t
    uint8_t *pBuffer;
    size_t   len;
    size_t   transferred;
} SDMATranferContext_t;

/** @brief main display driver context */
typedef struct
{
    bool                 isEnabled;
    EDisplayMode_t       mode;
    SDMATranferContext_t dmaTransferContext;
    bool                 DMAIsEnabled;
    bool                 DMAInProgress;

    UFrameBuffer_t      *pFrontBuffer;
    UFrameBuffer_t      *pBackBuffer;
    bool                 fbModified;
    uint8_t              currentX;
    uint8_t              currentY;
} SDisplayContext_t;

//=====================================================================================================================
// Globals
//=====================================================================================================================

SFrameBuffer_t           g_framebuffers;
static SDisplayContext_t g_displayContext = { 0 };

/** @brief initialization sequence for SSD1309 in Mode 5 */
SDisplayCommand_t SSD1309_INIT_SEQ[] = {
    (SDisplayCommand_t){ SSD1309_DISPLAY_OFF,           0x00, false },

    (SDisplayCommand_t){ SSD1309_SET_DISPLAY_CLOCK_DIV, 0xA0,
                        true                                        }, // clock divide ratio (0x00=1) and oscillator frequency (0x8)
    (SDisplayCommand_t){ SSD1309_SET_MULTIPLEX,         0x3F, true  },
    (SDisplayCommand_t){ SSD1309_SET_DISPLAY_OFFSET,    0x00, true  },
    (SDisplayCommand_t){ SSD1309_SET_START_LINE,        0x00, false },
    (SDisplayCommand_t){ SSD1309_SET_MASTER_CONFIG,     0x8E, true  },
    (SDisplayCommand_t){ SSD1309_MEMORY_MODE,           0x00, true  },
    (SDisplayCommand_t){ SSD1309_SEG_REMAP_FLIP,        0x00, false },
    (SDisplayCommand_t){ SSD1309_COM_SCAN_DEC,          0x00, false },
    (SDisplayCommand_t){ SSD1309_SET_COM_PINS,          0x12,
                        true                                        }, // alternative com pin config (bit 4), disable left/right remap (bit 5) -> datasheet
    // option 5 with COM_SCAN_INC, 8 with COM_SCAN_DEC
    (SDisplayCommand_t){ SSD1309_SET_CONTRAST,          0x6F, true  },
    (SDisplayCommand_t){ SSD1309_SET_PRECHARGE,         0xF1, true  }, // precharge period 0end.end.x/F1
    (SDisplayCommand_t){ SSD1309_SET_VCOM_DESELECT,     0x30, true  }, // vcomh deselect level

    (SDisplayCommand_t){ SSD1309_DEACTIVATE_SCROLL,     0x00, false },
    (SDisplayCommand_t){ SSD1309_DISPLAY_ALL_ON_RESUME, 0x00, false }, // normal mode: ram -> display
    (SDisplayCommand_t){ SSD1309_NORMAL_DISPLAY,        0x00, false }, // non-inverted mode
};

/** @brief command sequence for flipped mode (default) */
static const SDisplayCommand_t SSD1309_FLIP0_SEQ[] = {
    // default mode
    (SDisplayCommand_t){ SSD1309_SEG_REMAP_FLIP, 0x00, false },
    (SDisplayCommand_t){ SSD1309_COM_SCAN_DEC,   0x00, false },
};

/** @brief command sequence for non-flipped mode */
static const SDisplayCommand_t SSD1309_FLIP1_SEQ[] = {
    (SDisplayCommand_t){ SSD1309_SEG_REMAP_NORMAL, 0x00, false },
    (SDisplayCommand_t){ SSD1309_COM_SCAN_INC,     0x00, false },
};

//=====================================================================================================================
// Static protos
//=====================================================================================================================

static void           dispWriteCommand(SDisplayCommand_t);
static void           dispSyncFramebuffer(void *);

static void           dispDMACallback(bool);

static void           drawArc(SPoint_t, uint8_t, uint16_t, uint16_t, uint16_t, uint16_t, uint16_t, uint8_t, EColor_t);

static inline int16_t fxp_sin(uint16_t);
static inline int16_t fxp_cos(uint16_t);

//=====================================================================================================================
// Functions
//=====================================================================================================================

/**
 * @brief initialize display driver; setup buffers, configure display
 *
 * @param displayMode SPI or I2C
 */
void initDisplay(EDisplayMode_t displayMode)
{
    g_displayContext.mode          = displayMode;
    g_displayContext.DMAInProgress = false;
    g_displayContext.DMAIsEnabled  = false;
    g_displayContext.isEnabled     = false;
    g_displayContext.fbModified    = false;
    g_displayContext.currentX      = 0;
    g_displayContext.currentY      = 0;

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
        registerTimerCallback(TIMER_FRAMERATE, dispSyncFramebuffer, nullptr);
        resetDisplay(true);
        hwDelayMs(10);
        resetDisplay(false);
        hwDelayMs(500);
    }

    for (size_t i = 0; i < sizeof(SSD1309_INIT_SEQ) / sizeof(SSD1309_INIT_SEQ[0]); i++)
    {
        dispWriteCommand(SSD1309_INIT_SEQ[i]);
    }

    g_displayContext.DMAIsEnabled = true;
    g_displayContext.isEnabled    = true;
    hwDelayMs(100);

    dispWriteCommand((SDisplayCommand_t){ SSD1309_DISPLAY_ON, 0x00, false });
}

/**
 * @brief turn display on or off
 *
 * @param enable if true, turns display on, else off
 */
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
 * @brief draw a pixel in the backbuffer
 *
 * @param pixel pixel to draw {x, y}. pixels are based on a 1,1 offset
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

    g_displayContext.fbModified = true;
}

/**
 * @brief draw a line with Bresenham's algorithm
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
        dispDrawPixel((SPixel_t){
        { x0 + 1, y0 + 1 },
        line.color
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

/**
 * @brief write a single character to the display
 *
 * @param ch character to write
 * @param font font to use
 * @param color white or black
 * @return char the written char, or 0 on end/error
 *
 * @note this function makes use of the internal currentX and currentY pointers. if you want to write a char to an
 * arbitrary location make sure to set these to the right location first using dispSetCursor()
 */
char dispWriteChar(char ch, FontDef font, EColor_t color)
{
    // verify ch is a valid ascii character: table goes from ' ' to '~'
    if (ch < 0x20 || ch > 0x7E)
    {
        return 0;
    }

    // check if we have space on the current line
    if ((SSD1309_WIDTH < (g_displayContext.currentX + font.FontWidth)) ||
        (SSD1309_HEIGHT < (g_displayContext.currentY + font.FontHeight)))
    {
        return 0;
    }

    for (size_t i = 0; i < font.FontHeight; i++)
    {
        uint16_t character = font.data[(ch - 0x20) * font.FontHeight + i];

        for (size_t j = 0; j < font.FontWidth; j++)
        {
            // AND with 0x8000 to check bits in reverse
            if ((character << j) & 0x8000)
            {
                // if true, we should set this pixel
                dispDrawPixel((SPixel_t){
                { g_displayContext.currentX + j, (g_displayContext.currentY + i) },
                color
                });
            }
            else
            {
                // if false, it's blank
                dispDrawPixel((SPixel_t){
                { g_displayContext.currentX + j, (g_displayContext.currentY + i) },
                !color
                });
            }
        }
    }

    g_displayContext.currentX += font.FontWidth;

    return ch;
}

/**
 * @brief write a string to the display
 *
 * @param str string to write
 * @param font font to use
 * @param color white or black
 * @return char the last written char that succeeded, or 0 on end/error
 *
 * @note this function makes use of the internal currentX and currentY pointers. if you want to write a string to an
 * arbitrary location make sure to set these to the right location first using dispSetCursor()
 */
char dispWriteString(const char *str, FontDef font, EColor_t color)
{
    // write until char routine returns 0
    while (*str)
    {
        if (dispWriteChar(*str, font, color) != *str)
        {
            return *str;
        }

        str++;
    }

    return *str;
}

/**
 * @brief set cursor location in config struct
 *
 * @param x x location
 * @param y y location
 */
void dispSetCursor(uint8_t x, uint8_t y)
{
    g_displayContext.currentX = x;
    g_displayContext.currentY = y;
}

/**
 * @brief draw a circle or an arc (partial circle). consolidates multiple functions
 * 
 * @param center 
 * @param radius 
 * @param start_deg 
 * @param sweep_deg 
 * @param seg_count 
 * @param dash_on_deg 
 * @param dash_off_deg 
 * @param thickness 
 * @param draw_caps 
 * @param fill 
 * @param color 
 */
void dispDrawCircleShape(SPoint_t center, uint8_t radius, uint16_t start_deg, uint16_t sweep_deg, uint8_t seg_count,
                         uint16_t dash_on_deg, uint16_t dash_off_deg, uint8_t thickness, bool draw_caps, bool fill,
                         EColor_t color)
{
    if (seg_count == 0 || radius == 0 || thickness == 0 || sweep_deg == 0) return;

    start_deg %= 360;
    if (sweep_deg > 360) sweep_deg = 360;

    if (fill)
    {
        // Draw filled circle using concentric circles with increasing radius
        for (uint8_t r = 0; r <= radius; ++r)
        {
            for (uint8_t t = 0; t < thickness; ++t)
            {
                drawArc(center, r, start_deg, sweep_deg, seg_count, dash_on_deg, dash_off_deg, t + 1, color);
            }
        }
    }
    else
    {
        // Draw just the arc (non-filled)
        for (uint8_t t = 0; t < thickness; ++t)
        {
            drawArc(center, radius + t, start_deg, sweep_deg, seg_count, dash_on_deg, dash_off_deg, t + 1, color);
        }
    }

    // Draw caps (start and end points)
    if (draw_caps)
    {
        uint16_t end_deg = (start_deg + sweep_deg) % 360;

        for (uint8_t t = 0; t < thickness; ++t)
        {
            int16_t xs = center.x + (radius * fxp_sin(start_deg)) / FIXED_POINT_MATH_SCALE;
            int16_t ys = center.y + (radius * fxp_cos(start_deg)) / FIXED_POINT_MATH_SCALE;
            int16_t xe = center.x + (radius * fxp_sin(end_deg)) / FIXED_POINT_MATH_SCALE;
            int16_t ye = center.y + (radius * fxp_cos(end_deg)) / FIXED_POINT_MATH_SCALE;

            dispDrawPixel((SPixel_t){
            { xs, ys },
            color
            });
            dispDrawPixel((SPixel_t){
            { xe, ye },
            color
            });

            ++radius; // expand caps with thickness
        }
    }
}

/**
 * @brief draw a polyline
 *
 * @param pVertices        array of vertices to draw between
 * @param countVertices    length of pVertices
 * @param color            white or black
 */
void dispDrawPolyline(const SPoint_t *pVertices, uint16_t countVertices, EColor_t color)
{
    uint16_t i;

    if (pVertices != NULL)
    {
        for (i = 1; i < countVertices; i++)
        {
            dispDrawLine((SLine_t){
            { pVertices[i - 1].x, pVertices[i - 1].y },
            { pVertices[i].x,     pVertices[i].y     },
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

/**
 * @brief draw a rectangle between two points
 *
 * @param start start point
 * @param end   end point
 * @param color white or black
 */
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

/**
 * @brief draw a filled rectangle between two points
 *
 * @param start start point
 * @param end   end point
 * @param color white or black
 */
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

/**
 * @brief draw a bitmap. convert images using https://javl.github.io/image2cpp/ for example
 * @todo write compression/decompression algorithm for bitmap blobs (heatshrink?)
 *
 * @param coords start coordinates for top-left pixel of bitmap
 * @param bitmap pointer to bitmap data
 * @param w      width of bitmap
 * @param h      height of bitmap
 * @param color  white or black
 */
void dispDrawBitmap(SPoint_t coords, const uint8_t *bitmap, uint8_t w, uint8_t h, EColor_t color)
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

/**
 * @brief helper function to draw an arc using cosine LUT and fixed-point arithmetic
 *
 * @param center
 * @param radius
 * @param start_deg
 * @param sweep_deg
 * @param seg_count
 * @param dash_on_deg
 * @param dash_off_deg
 * @param thickness
 * @param color
 */
static void drawArc(SPoint_t center, uint8_t radius, uint16_t start_deg, uint16_t sweep_deg, uint16_t seg_count,
                    uint16_t dash_on_deg, uint16_t dash_off_deg, uint8_t thickness, EColor_t color)
{
    uint16_t step_deg   = sweep_deg / seg_count;
    uint16_t dash_total = dash_on_deg + dash_off_deg;
    uint16_t dash_pos   = 0;
    uint16_t angle      = start_deg;

    for (uint16_t i = 0; i < seg_count; ++i)
    {
        bool draw = true;

        if (dash_total)
        {
            dash_pos = (dash_pos + step_deg) % dash_total;
            draw     = (dash_pos < dash_on_deg);
        }

        if (!draw)
        {
            angle += step_deg;
            continue;
        }

        int16_t x1 = center.x + (radius * fxp_sin(angle)) / FIXED_POINT_MATH_SCALE;
        int16_t y1 = center.y + (radius * fxp_cos(angle)) / FIXED_POINT_MATH_SCALE;
        int16_t x2 = center.x + (radius * fxp_sin(angle + step_deg)) / FIXED_POINT_MATH_SCALE;
        int16_t y2 = center.y + (radius * fxp_cos(angle + step_deg)) / FIXED_POINT_MATH_SCALE;

        dispDrawLine((SLine_t){
        { x1, y1 },
        { x2, y2 },
        color
        });

        angle += step_deg;
    }
}

/**
 * @brief write a command to the display controller
 *
 * @param cmd command structure
 */
static void dispWriteCommand(SDisplayCommand_t cmd)
{
    uint8_t buf[2] = { cmd.command };
    if (cmd.hasParameter)
    {
        buf[1] = cmd.parameter;
    }
    spiSendCommand(buf, (cmd.hasParameter ? 2 : 1));
}

/**
 * @brief yeet the framebuffer to the display using DMA
 *
 */
static void dispSyncFramebuffer(void *pCtx)
{
    if (!g_displayContext.isEnabled || !g_displayContext.DMAIsEnabled || !g_displayContext.fbModified || g_displayContext.DMAInProgress) return;

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

/**
 * @brief DMA transfer complete callback
 *
 * @param isComplete (unused) true if TC, false if ERROR
 */
static void dispDMACallback(bool isComplete)
{
    (void)isComplete;
    // swap buffers for vsync
    memcpy(g_displayContext.pBackBuffer->buffer, g_displayContext.pFrontBuffer->buffer, SSD1309_GDDRAM_SIZE_BYTES);
    UFrameBuffer_t *pTmp                        = g_displayContext.pFrontBuffer;
    g_displayContext.pFrontBuffer               = g_displayContext.pBackBuffer;
    g_displayContext.pBackBuffer                = pTmp;
    g_displayContext.dmaTransferContext.pBuffer = g_displayContext.pFrontBuffer->buffer;
    g_displayContext.DMAInProgress              = false;
    g_displayContext.fbModified                 = false;
}

//=====================================================================================================================
// Fixed-point math helpers
//=====================================================================================================================

static inline int16_t fxp_sin(uint16_t deg)
{
    return fxp_cos((deg + 90) % 360);
}

static inline int16_t fxp_cos(uint16_t deg)
{
    deg %= 360;
    uint8_t idx = deg / 5;  // Direct index lookup for 5-degree increments
    int16_t val = g_cosLUT[idx];
    return (deg > 90 && deg <= 270) ? -val : val;
}