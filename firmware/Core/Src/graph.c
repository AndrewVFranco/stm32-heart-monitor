//
// Created by Andrew Franco on 1/24/26.
//
#include "graph.h"
#include <stdint.h>
#include "main.h"
#include "ili9341.h"
#include "FreeRTOS.h"
#include "semphr.h"

const uint8_t downsample_rate = 7;
const float amplitude = 1.0f;
static uint32_t graph_sum = 0;
static uint8_t graph_count = 0;
static uint8_t dma_buffer[480];
static uint16_t x_pos = 0;
static uint16_t last_y_pos = 120;
static uint8_t hold_count = 10;
static uint16_t line_color = 0x07E0;

extern SPI_HandleTypeDef hspi1;
extern SemaphoreHandle_t lcdSpiSemaphore;
extern volatile uint8_t beat_detected_debug_flag;

void Process_Graph(uint16_t raw_value) {
    graph_sum += raw_value;
    graph_count++;

    // 2. Process only when we have enough samples
    if (graph_count >= downsample_rate) {
        uint16_t val_to_draw = graph_sum / downsample_rate;

        // Reset Accumulators
        graph_sum = 0;
        graph_count = 0;

        // --- DRAWING LOGIC STARTS HERE ---

        // A. Calculate Y Position
        int32_t centered = (int32_t)val_to_draw - 2048;
        centered *= amplitude;
        int16_t y = 120 - (centered * 120 / 2048);

        // Clamp
        if (y < 51) y = 51;
        if (y > 239) y = 239;

        // B. Erase Old Line (Black Vertical Line)
        uint16_t erase_x = x_pos + 5;
        if (erase_x >= 320) erase_x -= 320;

        if (xSemaphoreTake(lcdSpiSemaphore, portMAX_DELAY) == pdTRUE) {
           ILI_DrawVerticalLine(erase_x, 51, 239, 0x0000);
           xSemaphoreGive(lcdSpiSemaphore);
        }

        // C. Prepare DMA Buffer (Color Calculation)
        uint16_t y1 = last_y_pos;
        uint16_t y2 = y;
        if (y1 > y2) { uint16_t temp = y1; y1 = y2; y2 = temp; }
        uint16_t height = y2 - y1 + 1;

        // Debug Flag Check (Red vs Green)
        if (beat_detected_debug_flag == 1) {
          if (hold_count > 0) {
            line_color = 0xF800; // RED
            hold_count--;
          }
          else {
            beat_detected_debug_flag = 0;
            line_color = 0x07E0; // Green
            hold_count = 10;
          }
        }

        for(int i=0; i < height * 2; i+=2) {
           dma_buffer[i]   = (line_color >> 8) & 0xFF;
           dma_buffer[i+1] = line_color & 0xFF;
        }

        // D. Send via DMA
        if (xSemaphoreTake(lcdSpiSemaphore, portMAX_DELAY) == pdTRUE) {
           // Set Window
           ILI_Write(0x2A, 1);
           uint8_t x_d[] = {x_pos>>8, x_pos&0xFF, x_pos>>8, x_pos&0xFF};
           HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
           HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
           HAL_SPI_Transmit(&hspi1, x_d, 4, 10);
           HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

           ILI_Write(0x2B, 1);
           uint8_t y_d[] = {y1>>8, y1&0xFF, y2>>8, y2&0xFF};
           HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
           HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
           HAL_SPI_Transmit(&hspi1, y_d, 4, 10);
           HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

           ILI_Write(0x2C, 1);
           HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
           HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
           for(volatile int k=0; k<10; k++);
           HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

           // Blast DMA
           if (HAL_SPI_Transmit_DMA(&hspi1, dma_buffer, height * 2) != HAL_OK) {
               HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
               xSemaphoreGive(lcdSpiSemaphore);
           }
        }

        // Advance Pointers
        last_y_pos = y;
        x_pos++;
        if (x_pos >= 320) x_pos = 0;
    }
}