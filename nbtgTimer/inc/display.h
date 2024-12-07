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

//=====================================================================================================================
// Defines
//=====================================================================================================================

#define SSD1309_COMMAND           0x80  // Continuation bit=1, D/C=0; 1000 0000
#define SSD1309_COMMAND_STREAM    0x00  // Continuation bit=0, D/C=0; 0000 0000
#define SSD1309_DATA              0xC0  // Continuation bit=1, D/C=1; 1100 0000
#define SSD1309_DATA_STREAM       0x40  // Continuation bit=0, D/C=1; 0100 0000
#define SSD1309_SET_MUX_RATIO     0xA8  // Set MUX ratio to N+1 MUX, N=A[5:0] : from 16MUX to 64MUX
#define SSD1309_DISPLAY_OFFSET    0xD3  // Set Display Offset
#define SSD1309_DISPLAY_ON        0xAF  // Display ON in normal mode
#define SSD1309_DISPLAY_OFF       0xAE  // Display OFF (sleep mode)
#define SSD1309_DIS_ENT_DISP_ON   0xA4  // Entire Display ON, Output ignores RAM content
#define SSD1309_DIS_IGNORE_RAM    0xA5  // Resume to RAM content display, Output follows RAM content
#define SSD1309_DIS_NORMAL        0xA6  // Normal display, 0 in RAM: OFF in display panel, 1 in RAM: ON in display panel
#define SSD1309_DIS_INVERSE       0xA7  // Inverse display, 0 in RAM: ON in display panel, 1 in RAM: OFF in display panel
#define SSD1309_DEACT_SCROLL      0x2E  // Stop scrolling that is configured by command 26h/27h/29h/2Ah
#define SSD1309_ACTIVE_SCROLL     0x2F  // Start scrolling that is configured by the scrolling setup commands:26h/27h/29h/2Ah
#define SSD1309_SET_START_LINE    0x40  // Set Display Start Line
#define SSD1309_MEMORY_ADDR_MODE  0x20  // Set Memory, Addressing Mode
#define SSD1309_SET_COLUMN_ADDR   0x21  // Set Column Address
#define SSD1309_SET_PAGE_ADDR     0x22  // Set Page Address 
#define SSD1309_SET_PAGE_START    0xB0  // page start address
#define SSD1309_SEG_REMAP         0xA0  // Set Segment Re-map, X[0]=0b column address 0 is mapped to SEG0
#define SSD1309_SEG_REMAP_OP      0xA1  // Set Segment Re-map, X[0]=1b: column address 127 is mapped to SEG0
#define SSD1309_COM_SCAN_DIR      0xC0  // Set COM Output, X[3]=0b: normal mode (RESET) Scan from COM0 to COM[N –1], e N is the Multiplex ratio
#define SSD1309_COM_SCAN_DIR_OP   0xC8  // Set COM Output, X[3]=1b: remapped mode. Scan from COM[N-1] to COM0, e N is the Multiplex ratio
#define SSD1309_COM_PIN_CONF      0xDA  // Set COM Pins Hardware Configuration, 
                                        // A[4]=0b, Sequential COM pin configuration, A[4]=1b(RESET), Alternative COM pin configuration
                                        // A[5]=0b(RESET), Disable COM Left/Right remap, A[5]=1b, Enable COM Left/Right remap                                        
#define SSD1309_SET_CONTRAST      0x81  // Set Contrast Control, Double byte command to select 1 to 256 contrast steps, increases as the value increases
#define SSD1309_SET_OSC_FREQ      0xD5  // Set Display Clock Divide Ratio/Oscillator Frequency
                                        // A[3:0] : Define the divide ratio (D) of the  display clocks (DCLK): Divide ratio= A[3:0] + 1, RESET is 0000b (divide ratio = 1) 
                                        // A[7:4] : Set the Oscillator Frequency, FOSC. Oscillator Frequency increases with the value of A[7:4] and vice versa. RESET is 1000b
#define SSD1309_SET_CHAR_REG      0x8D  // Charge Pump Setting, A[2] = 0b, Disable charge pump(RESET), A[2] = 1b, Enable charge pump during display on 
                                        // The Charge Pump must be enabled by the following command:
                                        // 8Dh ; Charge Pump Setting
                                        // 14h ; Enable Charge Pump
                                        // AFh; Display ON
#define SSD1309_SET_PRECHARGE     0xD9  // Set Pre-charge Period
#define SSD1309_VCOM_DESELECT     0xDB  // Set VCOMH Deselect Level
#define SSD1309_NOP               0xE3  // No operation
#define SSD1309_RESET             0xE4  // Maybe SW RESET, @source https://github.com/SmingHub/Sming/issues/501

//=====================================================================================================================
// Functions
//=====================================================================================================================

void initDisplay(void);

void ssd1306_Reset(void);
void ssd1306_WriteSingle(uint8_t);
void ssd1306_WriteMulti(uint8_t *, size_t);
void ssd1306_delayMs(uint32_t);

#ifdef __cplusplus
}
#endif
#endif //!_DISPLAY_H_