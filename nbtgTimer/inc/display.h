/**
 * @file  display.h
 * @brief 128x64 oled display routines
 */

#ifndef _DISPLAY_H_
#define _DISPLAY_H_

#ifdef __cplusplus
extern "C"
{
#endif

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

#include "display/fonts.h"

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define SSD1309_SET_START_LINE                       0x40 // sets start line to 0
#define SSD1309_MEMORY_MODE                          0x20 // 0x02 [reset] 0x00 - Horizontal addressing; 0x01 - Vertical addressing 0x02 - Page Addressing; 0x03 - Invalid
#define SSD1309_COLUMN_ADDR                          0x21 // used with horizontal or vertical addressing: { 0x21 0x00 0x7F } selects columns 0 to 127
#define SSD1309_PAGE_ADDR                            0x22 // used with horizontal or vertical addressing: { 0x22 0x00 0x07 } selects pages 0 to 7

#define SSD1309_RIGHT_HORIZONTAL_SCROLL              0x26
#define SSD1309_LEFT_HORIZONTAL_SCROLL               0x27
#define SSD1309_VERTICAL_AND_RIGHT_HORIZONTAL_SCROLL 0x29
#define SSD1309_VERTICAL_AND_LEFT_HORIZONTAL_SCROLL  0x2A
#define SSD1309_DEACTIVATE_SCROLL                    0x2E
#define SSD1309_ACTIVATE_SCROLL                      0x2F

#define SSD1309_SET_CONTRAST                         0x81 // 0x7F [reset]
#define SSD1309_CHARGE_PUMP                          0x8D

#define SSD1309_SET_VERTICAL_SCROLL_AREA             0xA3
#define SSD1309_DISPLAY_ALL_ON_RESUME                0xA4
#define SSD1309_DISPLAY_ALL_ON_IGNORE                0xA5

#define SSD1309_NORMAL_DISPLAY                       0xA6
#define SSD1309_INVERT_DISPLAY                       0xA7

#define SSD1309_SET_MULTIPLEX                        0xA8
#define SSD1309_SET_MASTER_CONFIG                    0xAD

#define SSD1309_DISPLAY_OFF                          0xAE
#define SSD1309_DISPLAY_ON                           0xAF

#define SSD1309_SEG_REMAP_NORMAL                     0xA0 // Used in conjunction with COM_SCAN_INC to rotate display such that Top of display is same side as the connection strip.
#define SSD1309_SEG_REMAP_FLIP                       0xA1 // Used in conjunction with COM_SCAN_DEC to rotate display such that Top of display is opposite side of the connection strip.
#define SSD1309_COM_SCAN_INC                         0xC0 // Normal Y axis.  (Top of display is same side as connection strip)
#define SSD1309_COM_SCAN_DEC                         0xC8 // Inverted Y axis (Top of display is opposite side of connection strip.

#define SSD1309_SET_DISPLAY_OFFSET                   0xD3 // sets the offset of the row data (wraps)
#define SSD1309_SET_DISPLAY_CLOCK_DIV                0xD5
#define SSD1309_SET_PRECHARGE                        0xD9 // 0x02 [reset]
#define SSD1309_SET_COM_PINS                         0xDA
#define SSD1309_SET_VCOM_DESELECT                    0xDB

#define SSD1309_COLUMN_START_ADDRESS_LOW_NIBBLE      0x00 // lower nibble: 0x00 and 0x10 make 0x00
#define SSD1309_COLUMN_START_ADDRESS_HI_NIBBLE       0x10 // higher nibble
#define SSD1309_SET_PAGE_START_ADDRESS               0xB0 // 0xB0 -> 0xB7 (page 0-7)

//=====================================================================================================================
// Types
//=====================================================================================================================

typedef enum
{
    MODE_I2C,
    MODE_SPI
} EDisplayMode_t;

typedef enum
{
    COLOR_BLACK,
    COLOR_WHITE
} EColor_t;

typedef struct
{
    uint8_t x;
    uint8_t y;
} SPoint_t;

typedef struct
{
    SPoint_t coordinates;
    EColor_t color;
} SPixel_t;

typedef struct
{
    SPoint_t start;
    SPoint_t end;
    EColor_t color;
} SLine_t;

//=====================================================================================================================
// Functions
//=====================================================================================================================

void initDisplay(EDisplayMode_t);
void toggleDisplay(bool);
void dispDrawPixel(SPixel_t);

void dispWriteSymbol(SymbolID_t Symbol, uint8_t x, uint8_t y);
char dispWriteChar(char ch, FontDef Font, EColor_t color);
char dispWriteString(const char* str, FontDef Font, EColor_t color);
void dispSetCursor(uint8_t x, uint8_t y);
void dispDrawLine(SLine_t);
void dispDrawArc(SPoint_t, uint8_t radius, uint16_t start_angle, uint16_t sweep, EColor_t color);
void dispDrawArcWithRadiusLine(uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep, EColor_t color);
void dispDrawCircle(SPoint_t, uint8_t, EColor_t, bool);
void dispFillCircle(uint8_t par_x,uint8_t par_y, uint8_t par_r, EColor_t par_color);
void dispPolyline(const SPoint_t *par_vertex, uint16_t par_size, EColor_t color);
void dispDrawRectangle(SPoint_t, SPoint_t, EColor_t);
void dispDrawFilledRectangle(SPoint_t start, SPoint_t end, EColor_t color);
void dispDrawBitmap(SPoint_t, const unsigned char*, uint8_t, uint8_t, EColor_t);


#ifdef __cplusplus
}
#endif
#endif //!_DISPLAY_H_