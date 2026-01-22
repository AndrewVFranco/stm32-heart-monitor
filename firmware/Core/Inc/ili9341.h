//
// Created by Andrew Franco on 1/2/26.
//

#ifndef STM32_HEART_MONITOR_ILI9348_H
#define STM32_HEART_MONITOR_ILI9348_H
/* Core/Inc/ili9341.h */

#include "main.h"
#include "fonts.h"

// Basic Colors
#define ILI_BLACK   0x0000
#define ILI_WHITE   0xFFFF
#define ILI_RED     0xF800
#define ILI_GREEN   0x07E0
#define ILI_BLUE    0x001F

// Function Prototypes
void ILI_Init_Minimal(void);
void ILI_Fill(uint16_t color);
void ILI_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ILI_DrawVerticalLine(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color);
void ILI_Write(uint8_t data, uint8_t is_cmd);
void ILI_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg, FontDef *font);
void ILI_WriteString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg, FontDef *font);
void ILI_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color);
void ILI_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

#endif //STM32_HEART_MONITOR_ILI9348_H