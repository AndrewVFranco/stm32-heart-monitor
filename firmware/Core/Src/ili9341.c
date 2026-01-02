//
// Created by Andrew Franco on 1/2/26.
//

/* Core/Src/ili9341.c */

#include "../Inc/ili9341.h"
#include "spi.h"  // Needed for hspi1
#include "gpio.h" // Needed for Pin definitions

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