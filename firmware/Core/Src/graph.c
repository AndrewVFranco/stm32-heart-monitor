//
// Created by Andrew Franco on 1/24/26.
//
#include "graph.h"
#include "main.h"
#include "ili9341.h"
#include "FreeRTOS.h"
#include "semphr.h"

// --- STM32F446RE CONFIGURATION ---
// DMA2, Stream 3, Channel 3 = SPI1_TX
#define GRAPH_DMA_STREAM    DMA2_Stream3
#define GRAPH_SPI_PORT      SPI1
#define GRAPH_DMA_CHANNEL   3

// Registers for DMA2 Stream 3
#define DMA_LIFCR_TCIF3     (1U << 27) // Transfer Complete
#define DMA_LIFCR_HTIF3     (1U << 26) // Half Transfer
#define DMA_LIFCR_TEIF3     (1U << 25) // Transfer Error
#define DMA_LIFCR_DMEIF3    (1U << 24) // Direct Mode Error
#define DMA_LIFCR_FEIF3     (1U << 22) // FIFO Error

// Import Handles from main.c for the Setup phase
extern SPI_HandleTypeDef hspi1;
extern SemaphoreHandle_t lcdSpiSemaphore;

// Variables
const uint8_t downsample_rate = 7;
const float amplitude = 1.0f;
static uint32_t graph_sum = 0;
static uint8_t graph_count = 0;
static uint8_t dma_buffer[480]; // Max column height (240) * 2 bytes
static uint16_t x_pos = 0;
static uint16_t last_y_pos = 120;
static uint16_t line_color = 0x07E0;

// HR Debug
// static uint8_t hold_count = 10;
// extern volatile uint8_t beat_detected_debug_flag;

void Process_Graph(uint16_t raw_value) {
    graph_sum += raw_value;
    graph_count++;

    if (graph_count >= downsample_rate) {
        uint16_t val_to_draw = graph_sum / downsample_rate;
        graph_sum = 0;
        graph_count = 0;

        // 1. Calc Y
        int32_t centered = (int32_t)val_to_draw - 2048;
        centered *= amplitude;
        int16_t y = 120 - (centered * 120 / 2048);
        if (y < 51) y = 51;
        if (y > 239) y = 239;

        // 2. Erase
        uint16_t erase_x = x_pos + 5;
        if (erase_x >= 320) erase_x -= 320;

        if (xSemaphoreTake(lcdSpiSemaphore, portMAX_DELAY) == pdTRUE) {
           ILI_DrawVerticalLine(erase_x, 51, 239, 0x0000);
           xSemaphoreGive(lcdSpiSemaphore);
        }

        // 3. Prepare Buffer
        uint16_t y1 = last_y_pos;
        uint16_t y2 = y;
        if (y1 > y2) { uint16_t temp = y1; y1 = y2; y2 = temp; }
        uint16_t height = y2 - y1 + 1;

        // HR Debug
        // if (beat_detected_debug_flag == 1) {
        //   if (hold_count > 0) { line_color = 0xF800; hold_count--; }
        //   else { beat_detected_debug_flag = 0; line_color = 0x07E0; hold_count = 10; }
        // }

        for(int i=0; i < height * 2; i+=2) {
           dma_buffer[i]   = (line_color >> 8) & 0xFF;
           dma_buffer[i+1] = line_color & 0xFF;
        }

        // 4. BARE METAL DMA TRANSFER
        if (xSemaphoreTake(lcdSpiSemaphore, portMAX_DELAY) == pdTRUE) {

           // A. Setup Window
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

           // B. Send 0x2C (Memory Write) - MANUAL REGISTER ACCESS
           // 1. Command Mode, CS Low
           HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_RESET);
           HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

           // 2. Send 0x2C to DR
           GRAPH_SPI_PORT->DR = 0x2C;

           // 3. Wait for TXE (Tx Empty) and BSY (Busy) to clear
           while (!(GRAPH_SPI_PORT->SR & SPI_SR_TXE));
           while (GRAPH_SPI_PORT->SR & SPI_SR_BSY);

           // 4. Data Mode (CS stays LOW!)
           HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);

           // C. Configure DMA
           GRAPH_DMA_STREAM->CR &= ~DMA_SxCR_EN;
           while(GRAPH_DMA_STREAM->CR & DMA_SxCR_EN);

           DMA2->LIFCR = DMA_LIFCR_TCIF3 | DMA_LIFCR_HTIF3 | DMA_LIFCR_TEIF3 | DMA_LIFCR_DMEIF3 | DMA_LIFCR_FEIF3;

           GRAPH_DMA_STREAM->M0AR = (uint32_t)dma_buffer;
           GRAPH_DMA_STREAM->PAR  = (uint32_t)&(GRAPH_SPI_PORT->DR);
           GRAPH_DMA_STREAM->NDTR = height * 2;

           uint32_t config = (GRAPH_DMA_CHANNEL << 25) | DMA_SxCR_MINC | DMA_SxCR_DIR_0 | DMA_SxCR_TCIE;
           GRAPH_DMA_STREAM->CR &= ~0x1FFFFFFF;
           GRAPH_DMA_STREAM->CR |= config;

           GRAPH_SPI_PORT->CR2 |= SPI_CR2_TXDMAEN;

           // D. Start DMA
           GRAPH_DMA_STREAM->CR |= DMA_SxCR_EN;
        }

        last_y_pos = y;
        x_pos++;
        if (x_pos >= 320) x_pos = 0;
    }
}
