//
// Created by Andrew Franco on 1/2/26.
//

#ifndef STM32_HEART_MONITOR_FONTS_H
#define STM32_HEART_MONITOR_FONTS_H

#include <stdint.h>

// Font Data Structure
typedef struct {
    const uint8_t width;       // Width of character in pixels
    const uint8_t height;      // Height of character in pixels
    const uint16_t *data;      // Pointer to the pixel data array
} FontDef;

// Small and large fonts
extern FontDef Font_7x10;
extern FontDef Font_11x18;

// Helper function to get string width for centering text
uint8_t GetFontWidth(FontDef *font);
uint8_t GetFontHeight(FontDef *font);

#endif //STM32_HEART_MONITOR_FONTS_H