//
// Created by Andrew Franco on 1/2/26.
//

/* Core/Src/ili9341.c */

#include "../Inc/ili9341.h"
#include "spi.h"
#include "gpio.h"
#include "fonts.h"

// External handle from spi.c
extern SPI_HandleTypeDef hspi1;

// --- LOW LEVEL ---

void ILI_Write(uint8_t data, uint8_t is_cmd) {
  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, is_cmd ? GPIO_PIN_RESET : GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, &data, 1, 100);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

// --- INITIALIZATION ---

void ILI_Init_Minimal() {
  // Hard Reset
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);
  HAL_Delay(100);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);
  HAL_Delay(100);

  // Init Sequence
  ILI_Write(0x01, 1); HAL_Delay(1000);

  ILI_Write(0xCB, 1); ILI_Write(0x39, 0); ILI_Write(0x2C, 0); ILI_Write(0x00, 0); ILI_Write(0x34, 0); ILI_Write(0x02, 0);
  ILI_Write(0xCF, 1); ILI_Write(0x00, 0); ILI_Write(0xC1, 0); ILI_Write(0x30, 0);
  ILI_Write(0xE8, 1); ILI_Write(0x85, 0); ILI_Write(0x00, 0); ILI_Write(0x78, 0);
  ILI_Write(0xEA, 1); ILI_Write(0x00, 0); ILI_Write(0x00, 0);
  ILI_Write(0xED, 1); ILI_Write(0x64, 0); ILI_Write(0x03, 0); ILI_Write(0x12, 0); ILI_Write(0x81, 0);
  ILI_Write(0xF7, 1); ILI_Write(0x20, 0);

  ILI_Write(0xC0, 1); ILI_Write(0x23, 0);
  ILI_Write(0xC1, 1); ILI_Write(0x10, 0);
  ILI_Write(0xC5, 1); ILI_Write(0x3E, 0); ILI_Write(0x28, 0);
  ILI_Write(0xC7, 1); ILI_Write(0x86, 0);

  ILI_Write(0x36, 1); ILI_Write(0xE8, 0); // Landscape
  ILI_Write(0x3A, 1); ILI_Write(0x55, 0);

  ILI_Write(0xB1, 1); ILI_Write(0x00, 0); ILI_Write(0x18, 0);
  ILI_Write(0xB6, 1); ILI_Write(0x08, 0); ILI_Write(0x82, 0); ILI_Write(0x27, 0);

  ILI_Write(0x11, 1); HAL_Delay(120);
  ILI_Write(0x29, 1); HAL_Delay(100);
}

// --- DRAWING FUNCTIONS ---

void ILI_Fill(uint16_t color) {
  // Set Window to Full Screen
  ILI_Write(0x2A, 1);
  uint8_t col_data[] = {0x00, 0x00, 0x01, 0x3F}; // 0-319
  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, col_data, 4, 100);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

  ILI_Write(0x2B, 1);
  uint8_t page_data[] = {0x00, 0x00, 0x00, 0xEF}; // 0-239
  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, page_data, 4, 100);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

  ILI_Write(0x2C, 1);

  // Burst Write
  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

  uint8_t high = color >> 8;
  uint8_t low = color & 0xFF;
  uint8_t line[20];

  for(int i=0; i<20; i+=2) { line[i] = high; line[i+1] = low; }

  for(long i=0; i<7680; i++) {
    HAL_SPI_Transmit(&hspi1, line, 20, 10);
  }

  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

void ILI_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
  if(x >= 320 || y >= 240) return;

  ILI_Write(0x2A, 1);
  uint8_t x_data[] = {x >> 8, x & 0xFF, x >> 8, x & 0xFF};
  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, x_data, 4, 10);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

  ILI_Write(0x2B, 1);
  uint8_t y_data[] = {y >> 8, y & 0xFF, y >> 8, y & 0xFF};
  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, y_data, 4, 10);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

  ILI_Write(0x2C, 1);
  uint8_t color_data[] = {color >> 8, color & 0xFF};
  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, color_data, 2, 10);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

void ILI_DrawVerticalLine(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color) {
  if (y1 > y2) { uint16_t temp = y1; y1 = y2; y2 = temp; }
  if (x >= 320) return;
  if (y1 >= 240) y1 = 0;
  if (y2 >= 240) y2 = 239;

  uint16_t height = y2 - y1 + 1;

  ILI_Write(0x2A, 1);
  uint8_t x_data[] = {x >> 8, x & 0xFF, x >> 8, x & 0xFF};
  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, x_data, 4, 10);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

  ILI_Write(0x2B, 1);
  uint8_t y_data[] = {y1 >> 8, y1 & 0xFF, y2 >> 8, y2 & 0xFF};
  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
  HAL_SPI_Transmit(&hspi1, y_data, 4, 10);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

  ILI_Write(0x2C, 1);

  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

  uint8_t high = color >> 8;
  uint8_t low = color & 0xFF;
  uint8_t buffer[20];
  for(int i=0; i<20; i+=2) { buffer[i] = high; buffer[i+1] = low; }

  int pixels_remaining = height;
  while(pixels_remaining > 0) {
     int pixels_to_send = (pixels_remaining > 10) ? 10 : pixels_remaining;
     HAL_SPI_Transmit(&hspi1, buffer, pixels_to_send * 2, 10);
     pixels_remaining -= pixels_to_send;
  }
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

/* * DRAW CHARACTER
 * x, y: Top-left coordinate
 * c: Character to draw
 * color: Text color
 * bg: Background color (pass 0x0000 for black)
 * font: Pointer to FontDef (e.g., &Font_7x10)
 */
void ILI_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint16_t bg, FontDef *font) {
    uint32_t i, b, j;

    // 1. Calculate the Offset in the font array
    // This depends on which font we are using!
    uint32_t position = 0;

    if (font->width == 7) {
        // Font_7x10 is standard ASCII (starts at space ' ' = 32)
        if (c < 32 || c > 126) return; // Invalid char
        position = (c - 32) * font->height; // Bytes per char is simply height (since width <= 8, 1 byte/row)
        // Wait! Font7x10 uses 16-bit array? Yes in your file.
        // 7x10 array uses 1 uint16_t per row.
    }
    else if (font->width == 11) {
        // Font_11x18 is "Sparse" (Digits only)
        // We map ASCII to our Array Indices manually:

        if (c >= '0' && c <= '9') {
            position = (c - '0') * font->height; // '0' is at index 0
        }
        else if (c == ' ') position = 10 * font->height; // Space is index 10
        else if (c == 'B') position = 11 * font->height;
        else if (c == 'P') position = 12 * font->height;
        else if (c == 'M') position = 13 * font->height;
        else if (c == '+') position = 14 * font->height; // Big Heart
        else if (c == '-') position = 15 * font->height; // NEW: Small Heart
        else return; // Unsupported character
    }

    // 2. Draw the pixels
    for (i = 0; i < font->height; i++) {
        // Read the row of pixels from flash
        // Each entry in our table is uint16_t
        uint16_t line = font->data[position + i];

        for (j = 0; j < font->width; j++) {
            // Check the bit corresponding to pixel j
            // We shift left: (0x8000 >> j) if width=16, but width is variable.
            // Let's assume Data is Left-Aligned in the 16-bit word.
            // Font7x10: 0xXX00 (Upper byte).
            // Font11x18: 0xXXX0.

            // Standard generic decoding:
            // Check bit: (line << j) & 0x8000 ?
            if ((line << j) & 0x8000) {
                ILI_DrawPixel(x + j, y + i, color);
            } else {
                ILI_DrawPixel(x + j, y + i, bg);
            }
        }
    }
}

// DRAW STRING
void ILI_WriteString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg, FontDef *font) {
    while (*str) {
        // Handle newline
        if (*str == '\n') {
            y += font->height;
            x = 0; // Reset X (or pass original X)
            str++;
            continue;
        }

        // Draw char
        ILI_DrawChar(x, y, *str, color, bg, font);

        // Advance X
        x += font->width;

        // Simple wrapping
        if (x + font->width >= 320) {
            x = 0;
            y += font->height;
        }

        str++;
    }
}

void ILI_DrawHLine(uint16_t x, uint16_t y, uint16_t w, uint16_t color) {
    ILI_FillRectangle(x, y, w, 1, color);
}

void ILI_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    // 1. Check bounds (optional but safe)
    if ((x >= 320) || (y >= 240)) return;
    if ((x + w - 1) >= 320) w = 320 - x;
    if ((y + h - 1) >= 240) h = 240 - y;

    // 2. Select the Block (Column Address Set)
    ILI_Write(0x2A, 1); // CASET
    uint8_t x_data[] = { x >> 8, x & 0xFF, (x + w - 1) >> 8, (x + w - 1) & 0xFF };
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, x_data, 4, 10);
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

    // 3. Select the Block (Page Address Set)
    ILI_Write(0x2B, 1); // PASET
    uint8_t y_data[] = { y >> 8, y & 0xFF, (y + h - 1) >> 8, (y + h - 1) & 0xFF };
    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&hspi1, y_data, 4, 10);
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

    // 4. Send Color Data (Memory Write)
    ILI_Write(0x2C, 1); // RAMWR

    uint32_t total_pixels = w * h;

    // Prepare a small buffer to speed things up (sending 2 bytes at a time is slow)
    // We will send chunks of pixels.
    #define BUF_SIZE 64
    uint8_t color_buffer[BUF_SIZE * 2];

    // Fill buffer with the target color
    for (int i = 0; i < BUF_SIZE; i++) {
        color_buffer[i * 2] = color >> 8;
        color_buffer[i * 2 + 1] = color & 0xFF;
    }

    HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

    // Blast the data
    while (total_pixels > 0) {
        uint32_t chunk = (total_pixels > BUF_SIZE) ? BUF_SIZE : total_pixels;
        HAL_SPI_Transmit(&hspi1, color_buffer, chunk * 2, 100);
        total_pixels -= chunk;
    }

    HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}