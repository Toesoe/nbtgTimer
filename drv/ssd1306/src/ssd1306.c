/**
 * @file ssd1306.c
 *
 * @brief simple SSD1306 driver. also supports SSD1309
 * @note provide an implementation for the weak functions
 */

//=====================================================================================================================
// Includes
//=====================================================================================================================

#include "ssd1306.h"

#include <math.h>
#include <stdlib.h>
#include <string.h> // For memcpy

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define SSD1306_COMMAND           0x80  // Continuation bit=1, D/C=0; 1000 0000
#define SSD1306_COMMAND_STREAM    0x00  // Continuation bit=0, D/C=0; 0000 0000
#define SSD1306_DATA              0xC0  // Continuation bit=1, D/C=1; 1100 0000
#define SSD1306_DATA_STREAM       0x40  // Continuation bit=0, D/C=1; 0100 0000
#define SSD1306_SET_MUX_RATIO     0xA8  // Set MUX ratio to N+1 MUX, N=A[5:0] : from 16MUX to 64MUX
#define SSD1306_DISPLAY_OFFSET    0xD3  // Set Display Offset
#define SSD1306_DISPLAY_ON        0xAF  // Display ON in normal mode
#define SSD1306_DISPLAY_OFF       0xAE  // Display OFF (sleep mode)
#define SSD1306_DIS_ENT_DISP_ON   0xA4  // Entire Display ON, Output ignores RAM content
#define SSD1306_DIS_IGNORE_RAM    0xA5  // Resume to RAM content display, Output follows RAM content
#define SSD1306_DIS_NORMAL        0xA6  // Normal display, 0 in RAM: OFF in display panel, 1 in RAM: ON in display panel
#define SSD1306_DIS_INVERSE       0xA7  // Inverse display, 0 in RAM: ON in display panel, 1 in RAM: OFF in display panel
#define SSD1306_DEACT_SCROLL      0x2E  // Stop scrolling that is configured by command 26h/27h/29h/2Ah
#define SSD1306_ACTIVE_SCROLL     0x2F  // Start scrolling that is configured by the scrolling setup commands:26h/27h/29h/2Ah
#define SSD1306_SET_START_LINE    0x40  // Set Display Start Line
#define SSD1306_MEMORY_ADDR_MODE  0x20  // Set Memory, Addressing Mode
#define SSD1306_SET_COLUMN_ADDR   0x21  // Set Column Address
#define SSD1306_SET_PAGE_ADDR     0x22  // Set Page Address 
#define SSD1306_SEG_REMAP         0xA0  // Set Segment Re-map, X[0]=0b column address 0 is mapped to SEG0
#define SSD1306_SEG_REMAP_OP      0xA1  // Set Segment Re-map, X[0]=1b: column address 127 is mapped to SEG0
#define SSD1306_COM_SCAN_DIR      0xC0  // Set COM Output, X[3]=0b: normal mode (RESET) Scan from COM0 to COM[N –1], e N is the Multiplex ratio
#define SSD1306_COM_SCAN_DIR_OP   0xC8  // Set COM Output, X[3]=1b: remapped mode. Scan from COM[N-1] to COM0, e N is the Multiplex ratio
#define SSD1306_COM_PIN_CONF      0xDA  // Set COM Pins Hardware Configuration, 
                                        // A[4]=0b, Sequential COM pin configuration, A[4]=1b(RESET), Alternative COM pin configuration
                                        // A[5]=0b(RESET), Disable COM Left/Right remap, A[5]=1b, Enable COM Left/Right remap
#define SSD1306_SET_CONTRAST      0x81  // Set Contrast Control, Double byte command to select 1 to 256 contrast steps, increases as the value increases
#define SSD1306_SET_OSC_FREQ      0xD5  // Set Display Clock Divide Ratio/Oscillator Frequency
                                        // A[3:0] : Define the divide ratio (D) of the  display clocks (DCLK): Divide ratio= A[3:0] + 1, RESET is 0000b (divide ratio = 1) 
                                        // A[7:4] : Set the Oscillator Frequency, FOSC. Oscillator Frequency increases with the value of A[7:4] and vice versa. RESET is 1000b
#define SSD1306_SET_CHAR_REG      0x8D  // Charge Pump Setting, A[2] = 0b, Disable charge pump(RESET), A[2] = 1b, Enable charge pump during display on 
                                        // The Charge Pump must be enabled by the following command:
                                        // 8Dh ; Charge Pump Setting
                                        // 14h ; Enable Charge Pump
                                        // AFh; Display ON
#define SSD1306_SET_PRECHARGE     0xD9  // Set Pre-charge Period
#define SSD1306_VCOM_DESELECT     0xDB  // Set VCOMH Deselect Leve
#define SSD1306_NOP               0xE3  // No operation
#define SSD1306_RESET             0xE4  // Maybe SW RESET, @source https://github.com/SmingHub/Sming/issues/501

//=====================================================================================================================
// Types
//=====================================================================================================================

//=====================================================================================================================
// Globals
//=====================================================================================================================

// Screenbuffer
static uint8_t SSD1306_Buffer[SSD1306_BUFFER_SIZE];

// Screen object
static SSD1306_t SSD1306;

//=====================================================================================================================
// Function prototypes
//=====================================================================================================================

__attribute__((weak)) ssd1306_Reset(void);
__attribute__((weak)) ssd1306_WriteSingle(uint8_t);
__attribute__((weak)) ssd1306_WriteMulti(uint8_t *, size_t);
__attribute__((weak)) ssd1306_delayMs(uint32_t);

//=====================================================================================================================
// External functions
//=====================================================================================================================


/* Fills the Screenbuffer with values from a given buffer of a fixed length */
SSD1306_Error_t ssd1306_FillBuffer(uint8_t *buf, uint32_t len)
{
    SSD1306_Error_t ret = SSD1306_ERR;
    if (len <= SSD1306_BUFFER_SIZE)
    {
        memcpy(SSD1306_Buffer, buf, len);
        ret = SSD1306_OK;
    }
    return ret;
}

/* Initialize the oled screen */
void ssd1306_Init(void)
{
    // Reset OLED
    ssd1306_Reset();

    // Wait for the screen to boot
    ssd1306_delayMs(100);

    // Init OLED
    ssd1306_SetDisplayOn(0); // display off

    ssd1306_writeSingle(0x20); // Set Memory Addressing Mode
    ssd1306_writeSingle(0x00); // 00b,Horizontal Addressing Mode; 01b,Vertical Addressing Mode;
                                // 10b,Page Addressing Mode (RESET); 11b,Invalid

    ssd1306_writeSingle(0xB0); // Set Page Start Address for Page Addressing Mode,0-7

#ifdef SSD1306_MIRROR_VERT
    ssd1306_writeSingle(0xC0); // Mirror vertically
#else
    ssd1306_writeSingle(0xC8); // Set COM Output Scan Direction
#endif

    ssd1306_writeSingle(0x00); //---set low column address
    ssd1306_writeSingle(0x10); //---set high column address

    ssd1306_writeSingle(0x40); //--set start line address - CHECK

    ssd1306_SetContrast(0xFF);

#ifdef SSD1306_MIRROR_HORIZ
    ssd1306_writeSingle(0xA0); // Mirror horizontally
#else
    ssd1306_writeSingle(0xA1); //--set segment re-map 0 to 127 - CHECK
#endif

#ifdef SSD1306_INVERSE_COLOR
    ssd1306_writeSingle(0xA7); //--set inverse color
#else
    ssd1306_writeSingle(0xA6); //--set normal color
#endif

// Set multiplex ratio.
#if (SSD1306_HEIGHT == 128)
    // Found in the Luma Python lib for SH1106.
    ssd1306_writeSingle(0xFF);
#else
    ssd1306_writeSingle(0xA8); //--set multiplex ratio(1 to 64) - CHECK
#endif

#if (SSD1306_HEIGHT == 32)
    ssd1306_writeSingle(0x1F); //
#elif (SSD1306_HEIGHT == 64)
    ssd1306_writeSingle(0x3F); //
#elif (SSD1306_HEIGHT == 128)
    ssd1306_writeSingle(0x3F); // Seems to work for 128px high displays too.
#else
#error "Only 32, 64, or 128 lines of height are supported!"
#endif

    ssd1306_writeSingle(0xA4); // 0xa4,Output follows RAM content;0xa5,Output ignores RAM content

    ssd1306_writeSingle(0xD3); //-set display offset - CHECK
    ssd1306_writeSingle(0x00); //-not offset

    ssd1306_writeSingle(0xD5); //--set display clock divide ratio/oscillator frequency
    ssd1306_writeSingle(0xF0); //--set divide ratio

    ssd1306_writeSingle(0xD9); //--set pre-charge period
    ssd1306_writeSingle(0x22); //

    ssd1306_writeSingle(0xDA); //--set com pins hardware configuration - CHECK
#if (SSD1306_HEIGHT == 32)
    ssd1306_writeSingle(0x02);
#elif (SSD1306_HEIGHT == 64)
    ssd1306_writeSingle(0x12);
#elif (SSD1306_HEIGHT == 128)
    ssd1306_writeSingle(0x12);
#else
#error "Only 32, 64, or 128 lines of height are supported!"
#endif

    ssd1306_writeSingle(0xDB); //--set vcomh
    ssd1306_writeSingle(0x20); // 0x20,0.77xVcc

    ssd1306_writeSingle(0x8D); //--set DC-DC enable
    ssd1306_writeSingle(0x14); //
    ssd1306_SetDisplayOn(1);    //--turn on SSD1306 panel

    // Clear screen
    ssd1306_Fill(Black);

    // Flush buffer to screen
    ssd1306_UpdateScreen();

    // Set default values for screen object
    SSD1306.CurrentX    = 0;
    SSD1306.CurrentY    = 0;

    SSD1306.Initialized = 1;
}

/* Fill the whole screen with the given color */
void ssd1306_Fill(SSD1306_COLOR color)
{
    memset(SSD1306_Buffer, (color == Black) ? 0x00 : 0xFF, sizeof(SSD1306_Buffer));
}

/* Write the screenbuffer with changed to the screen */
void ssd1306_UpdateScreen(void)
{
    // Write data to each page of RAM. Number of pages
    // depends on the screen height:
    //
    //  * 32px   ==  4 pages
    //  * 64px   ==  8 pages
    //  * 128px  ==  16 pages
    for (uint8_t i = 0; i < SSD1306_HEIGHT / 8; i++)
    {
        ssd1306_writeSingle(0xB0 + i); // Set the current RAM page address.
        ssd1306_writeSingle(0x00 + SSD1306_X_OFFSET_LOWER);
        ssd1306_writeSingle(0x10 + SSD1306_X_OFFSET_UPPER);
        ssd1306_WriteMulti(&SSD1306_Buffer[SSD1306_WIDTH * i], SSD1306_WIDTH);
    }
}

/*
 * Draw one pixel in the screenbuffer
 * X => X Coordinate
 * Y => Y Coordinate
 * color => Pixel color
 */
void ssd1306_DrawPixel(uint8_t x, uint8_t y, SSD1306_COLOR color)
{
    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT)
    {
        // Don't write outside the buffer
        return;
    }

    // Draw in the right color
    if (color == White)
    {
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] |= 1 << (y % 8);
    }
    else
    {
        SSD1306_Buffer[x + (y / 8) * SSD1306_WIDTH] &= ~(1 << (y % 8));
    }
}

/*
 * Draw 1 char to the screen buffer
 * ch       => char om weg te schrijven
 * Font     => Font waarmee we gaan schrijven
 * color    => Black or White
 */
char ssd1306_WriteChar(char ch, SSD1306_Font_t Font, SSD1306_COLOR color)
{
    uint32_t i, b, j;

    // Check if character is valid
    if (ch < 32 || ch > 126) return 0;

    // Check remaining space on current line
    if (SSD1306_WIDTH < (SSD1306.CurrentX + Font.width) || SSD1306_HEIGHT < (SSD1306.CurrentY + Font.height))
    {
        // Not enough space on current line
        return 0;
    }

    // Use the font to write
    for (i = 0; i < Font.height; i++)
    {
        b = Font.data[(ch - 32) * Font.height + i];
        for (j = 0; j < Font.width; j++)
        {
            if ((b << j) & 0x8000)
            {
                ssd1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR)color);
            }
            else
            {
                ssd1306_DrawPixel(SSD1306.CurrentX + j, (SSD1306.CurrentY + i), (SSD1306_COLOR)!color);
            }
        }
    }

    // The current space is now taken
    SSD1306.CurrentX += Font.char_width ? Font.char_width[ch - 32] : Font.width;

    // Return written char for validation
    return ch;
}

/* Write full string to screenbuffer */
char ssd1306_WriteString(char *str, SSD1306_Font_t Font, SSD1306_COLOR color)
{
    while (*str)
    {
        if (ssd1306_WriteChar(*str, Font, color) != *str)
        {
            // Char could not be written
            return *str;
        }
        str++;
    }

    // Everything ok
    return *str;
}

/* Position the cursor */
void ssd1306_SetCursor(uint8_t x, uint8_t y)
{
    SSD1306.CurrentX = x;
    SSD1306.CurrentY = y;
}

/* Draw line by Bresenhem's algorithm */
void ssd1306_Line(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color)
{
    int32_t deltaX = abs(x2 - x1);
    int32_t deltaY = abs(y2 - y1);
    int32_t signX  = ((x1 < x2) ? 1 : -1);
    int32_t signY  = ((y1 < y2) ? 1 : -1);
    int32_t error  = deltaX - deltaY;
    int32_t error2;

    ssd1306_DrawPixel(x2, y2, color);

    while ((x1 != x2) || (y1 != y2))
    {
        ssd1306_DrawPixel(x1, y1, color);
        error2 = error * 2;
        if (error2 > -deltaY)
        {
            error -= deltaY;
            x1 += signX;
        }

        if (error2 < deltaX)
        {
            error += deltaX;
            y1 += signY;
        }
    }
    return;
}

/* Draw polyline */
void ssd1306_Polyline(const SSD1306_VERTEX *par_vertex, uint16_t par_size, SSD1306_COLOR color)
{
    uint16_t i;
    if (par_vertex == NULL)
    {
        return;
    }

    for (i = 1; i < par_size; i++)
    {
        ssd1306_Line(par_vertex[i - 1].x, par_vertex[i - 1].y, par_vertex[i].x, par_vertex[i].y, color);
    }

    return;
}

/* Convert Degrees to Radians */
static float ssd1306_DegToRad(float par_deg)
{
    return par_deg * (3.14f / 180.0f);
}

/* Normalize degree to [0;360] */
static uint16_t ssd1306_NormalizeTo0_360(uint16_t par_deg)
{
    uint16_t loc_angle;
    if (par_deg <= 360)
    {
        loc_angle = par_deg;
    }
    else
    {
        loc_angle = par_deg % 360;
        loc_angle = (loc_angle ? loc_angle : 360);
    }
    return loc_angle;
}

/*
 * DrawArc. Draw angle is beginning from 4 quart of trigonometric circle (3pi/2)
 * start_angle in degree
 * sweep in degree
 */
void ssd1306_DrawArc(uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep, SSD1306_COLOR color)
{
    static const uint8_t CIRCLE_APPROXIMATION_SEGMENTS = 36;
    float                approx_degree;
    uint32_t             approx_segments;
    uint8_t              xp1, xp2;
    uint8_t              yp1, yp2;
    uint32_t             count;
    uint32_t             loc_sweep;
    float                rad;

    loc_sweep       = ssd1306_NormalizeTo0_360(sweep);

    count           = (ssd1306_NormalizeTo0_360(start_angle) * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_segments = (loc_sweep * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_degree   = loc_sweep / (float)approx_segments;
    while (count < approx_segments)
    {
        rad = ssd1306_DegToRad(count * approx_degree);
        xp1 = x + (int8_t)(sinf(rad) * radius);
        yp1 = y + (int8_t)(cosf(rad) * radius);
        count++;
        if (count != approx_segments)
        {
            rad = ssd1306_DegToRad(count * approx_degree);
        }
        else
        {
            rad = ssd1306_DegToRad(loc_sweep);
        }
        xp2 = x + (int8_t)(sinf(rad) * radius);
        yp2 = y + (int8_t)(cosf(rad) * radius);
        ssd1306_Line(xp1, yp1, xp2, yp2, color);
    }

    return;
}

/*
 * Draw arc with radius line
 * Angle is beginning from 4 quart of trigonometric circle (3pi/2)
 * start_angle: start angle in degree
 * sweep: finish angle in degree
 */
void ssd1306_DrawArcWithRadiusLine(uint8_t x, uint8_t y, uint8_t radius, uint16_t start_angle, uint16_t sweep,
                                   SSD1306_COLOR color)
{
    const uint32_t CIRCLE_APPROXIMATION_SEGMENTS = 36;
    float          approx_degree;
    uint32_t       approx_segments;
    uint8_t        xp1;
    uint8_t        xp2 = 0;
    uint8_t        yp1;
    uint8_t        yp2 = 0;
    uint32_t       count;
    uint32_t       loc_sweep;
    float          rad;

    loc_sweep             = ssd1306_NormalizeTo0_360(sweep);

    count                 = (ssd1306_NormalizeTo0_360(start_angle) * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_segments       = (loc_sweep * CIRCLE_APPROXIMATION_SEGMENTS) / 360;
    approx_degree         = loc_sweep / (float)approx_segments;

    rad                   = ssd1306_DegToRad(count * approx_degree);
    uint8_t first_point_x = x + (int8_t)(sinf(rad) * radius);
    uint8_t first_point_y = y + (int8_t)(cosf(rad) * radius);
    while (count < approx_segments)
    {
        rad = ssd1306_DegToRad(count * approx_degree);
        xp1 = x + (int8_t)(sinf(rad) * radius);
        yp1 = y + (int8_t)(cosf(rad) * radius);
        count++;
        if (count != approx_segments)
        {
            rad = ssd1306_DegToRad(count * approx_degree);
        }
        else
        {
            rad = ssd1306_DegToRad(loc_sweep);
        }
        xp2 = x + (int8_t)(sinf(rad) * radius);
        yp2 = y + (int8_t)(cosf(rad) * radius);
        ssd1306_Line(xp1, yp1, xp2, yp2, color);
    }

    // Radius line
    ssd1306_Line(x, y, first_point_x, first_point_y, color);
    ssd1306_Line(x, y, xp2, yp2, color);
    return;
}

/* Draw circle by Bresenhem's algorithm */
void ssd1306_DrawCircle(uint8_t par_x, uint8_t par_y, uint8_t par_r, SSD1306_COLOR par_color)
{
    int32_t x   = -par_r;
    int32_t y   = 0;
    int32_t err = 2 - 2 * par_r;
    int32_t e2;

    if (par_x >= SSD1306_WIDTH || par_y >= SSD1306_HEIGHT)
    {
        return;
    }

    do
    {
        ssd1306_DrawPixel(par_x - x, par_y + y, par_color);
        ssd1306_DrawPixel(par_x + x, par_y + y, par_color);
        ssd1306_DrawPixel(par_x + x, par_y - y, par_color);
        ssd1306_DrawPixel(par_x - x, par_y - y, par_color);
        e2 = err;

        if (e2 <= y)
        {
            y++;
            err = err + (y * 2 + 1);
            if (-x == y && e2 <= x)
            {
                e2 = 0;
            }
        }

        if (e2 > x)
        {
            x++;
            err = err + (x * 2 + 1);
        }
    }
    while (x <= 0);

    return;
}

/* Draw filled circle. Pixel positions calculated using Bresenham's algorithm */
void ssd1306_FillCircle(uint8_t par_x, uint8_t par_y, uint8_t par_r, SSD1306_COLOR par_color)
{
    int32_t x   = -par_r;
    int32_t y   = 0;
    int32_t err = 2 - 2 * par_r;
    int32_t e2;

    if (par_x >= SSD1306_WIDTH || par_y >= SSD1306_HEIGHT)
    {
        return;
    }

    do
    {
        for (uint8_t _y = (par_y + y); _y >= (par_y - y); _y--)
        {
            for (uint8_t _x = (par_x - x); _x >= (par_x + x); _x--)
            {
                ssd1306_DrawPixel(_x, _y, par_color);
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
        }

        if (e2 > x)
        {
            x++;
            err = err + (x * 2 + 1);
        }
    }
    while (x <= 0);

    return;
}

/* Draw a rectangle */
void ssd1306_DrawRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color)
{
    ssd1306_Line(x1, y1, x2, y1, color);
    ssd1306_Line(x2, y1, x2, y2, color);
    ssd1306_Line(x2, y2, x1, y2, color);
    ssd1306_Line(x1, y2, x1, y1, color);

    return;
}

/* Draw a filled rectangle */
void ssd1306_FillRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2, SSD1306_COLOR color)
{
    uint8_t x_start = ((x1 <= x2) ? x1 : x2);
    uint8_t x_end   = ((x1 <= x2) ? x2 : x1);
    uint8_t y_start = ((y1 <= y2) ? y1 : y2);
    uint8_t y_end   = ((y1 <= y2) ? y2 : y1);

    for (uint8_t y = y_start; (y <= y_end) && (y < SSD1306_HEIGHT); y++)
    {
        for (uint8_t x = x_start; (x <= x_end) && (x < SSD1306_WIDTH); x++)
        {
            ssd1306_DrawPixel(x, y, color);
        }
    }
    return;
}

SSD1306_Error_t ssd1306_InvertRectangle(uint8_t x1, uint8_t y1, uint8_t x2, uint8_t y2)
{
    if ((x2 >= SSD1306_WIDTH) || (y2 >= SSD1306_HEIGHT))
    {
        return SSD1306_ERR;
    }
    if ((x1 > x2) || (y1 > y2))
    {
        return SSD1306_ERR;
    }
    uint32_t i;
    if ((y1 / 8) != (y2 / 8))
    {
        /* if rectangle doesn't lie on one 8px row */
        for (uint32_t x = x1; x <= x2; x++)
        {
            i = x + (y1 / 8) * SSD1306_WIDTH;
            SSD1306_Buffer[i] ^= 0xFF << (y1 % 8);
            i += SSD1306_WIDTH;
            for (; i < x + (y2 / 8) * SSD1306_WIDTH; i += SSD1306_WIDTH)
            {
                SSD1306_Buffer[i] ^= 0xFF;
            }
            SSD1306_Buffer[i] ^= 0xFF >> (7 - (y2 % 8));
        }
    }
    else
    {
        /* if rectangle lies on one 8px row */
        const uint8_t mask = (0xFF << (y1 % 8)) & (0xFF >> (7 - (y2 % 8)));
        for (i = x1 + (y1 / 8) * SSD1306_WIDTH; i <= (uint32_t)x2 + (y2 / 8) * SSD1306_WIDTH; i++)
        {
            SSD1306_Buffer[i] ^= mask;
        }
    }
    return SSD1306_OK;
}

/* Draw a bitmap */
void ssd1306_DrawBitmap(uint8_t x, uint8_t y, const unsigned char *bitmap, uint8_t w, uint8_t h, SSD1306_COLOR color)
{
    int16_t byteWidth = (w + 7) / 8; // Bitmap scanline pad = whole byte
    uint8_t byte      = 0;

    if (x >= SSD1306_WIDTH || y >= SSD1306_HEIGHT)
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
                ssd1306_DrawPixel(x + i, y, color);
            }
        }
    }
    return;
}

void ssd1306_SetContrast(const uint8_t value)
{
    const uint8_t kSetContrastControlRegister = 0x81;
    ssd1306_writeSingle(kSetContrastControlRegister);
    ssd1306_writeSingle(value);
}

void ssd1306_SetDisplayOn(const uint8_t on)
{
    uint8_t value;
    if (on)
    {
        value             = 0xAF; // Display on
        SSD1306.DisplayOn = 1;
    }
    else
    {
        value             = 0xAE; // Display off
        SSD1306.DisplayOn = 0;
    }
    ssd1306_writeSingle(value);
}

uint8_t ssd1306_GetDisplayOn()
{
    return SSD1306.DisplayOn;
}