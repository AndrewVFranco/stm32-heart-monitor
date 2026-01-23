/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"
#include "PanTompkins.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "ili9341.h"
#include "filter.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

// --- SHARED DATA ---
volatile uint8_t global_bpm = 0;

uint8_t hold_count = 10;
uint16_t line_color = 0x07E0; // Default Green

// --- PAN-TOMPKINS SETTINGS ---
#define WINDOW_SIZE 50
#define MAX_SLOPE 150
#define SLOPE_THRESHOLD 20000
#define ASYSTOLE_MS 3000

const float amplitude = 1.0f;
volatile uint8_t beat_detected_debug_flag = 0;

// --- HARDWARE HANDLES (Defined in main.c) ---
extern ADC_HandleTypeDef hadc1;
extern SPI_HandleTypeDef hspi1;

// --- RTOS OBJECTS ---
QueueHandle_t ecgQueue;
SemaphoreHandle_t lcdSpiSemaphore;

// --- DISPLAY STATE ---
uint16_t x_pos = 0;
uint16_t last_y_pos = 120;
uint16_t height = 240;

/* USER CODE END Variables */
osThreadId graphTaskHandle;
osThreadId sensorTaskHandle;
osThreadId UITaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void StartTask02(void const * argument);
void StartTask03(void const * argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/* GetIdleTaskMemory prototype (linked to static allocation support) */
void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );

/* USER CODE BEGIN GET_IDLE_TASK_MEMORY */
static StaticTask_t xIdleTaskTCBBuffer;
static StackType_t xIdleStack[configMINIMAL_STACK_SIZE];

void vApplicationGetIdleTaskMemory( StaticTask_t **ppxIdleTaskTCBBuffer, StackType_t **ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &xIdleTaskTCBBuffer;
  *ppxIdleTaskStackBuffer = &xIdleStack[0];
  *pulIdleTaskStackSize = configMINIMAL_STACK_SIZE;
  /* place for user code */
}
/* USER CODE END GET_IDLE_TASK_MEMORY */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  ecgQueue = xQueueCreate(128, sizeof(uint16_t));

  // Create Semaphore (Binary)
  lcdSpiSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(lcdSpiSemaphore); // Give it once so it starts "Unlocked"

  // Start ADC
  HAL_ADC_Start(&hadc1);

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of graphTask */
  osThreadDef(graphTask, StartDefaultTask, osPriorityNormal, 0, 512);
  graphTaskHandle = osThreadCreate(osThread(graphTask), NULL);

  /* definition and creation of sensorTask */
  osThreadDef(sensorTask, StartTask02, osPriorityRealtime, 0, 256);
  sensorTaskHandle = osThreadCreate(osThread(sensorTask), NULL);

  /* definition and creation of UITask */
  osThreadDef(UITask, StartTask03, osPriorityLow, 0, 128);
  UITaskHandle = osThreadCreate(osThread(UITask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the displayTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void const * argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  PT_init();
  uint16_t val_from_queue;
  uint32_t current_time;

  // --- GRAPH SIGNAL PROCESSING VARIABLES ---
  uint32_t graph_average_sum = 0;
  uint8_t graph_count = 0;
  const uint8_t graph_downsample = 7;

  // --- HR CALCULATION ---
  #define BPM_BUFFER_SIZE 5
  uint32_t bpm_buffer[BPM_BUFFER_SIZE] = {0};
  uint8_t bpm_idx = 0;
  uint32_t last_beat_time = 0;

  uint32_t hr_sum = 0;
  uint8_t hr_count = 0;
  uint16_t hr_downsample = 5;

  // --- DISPLAY VARIABLES ---
  static uint8_t dma_buffer[480]; // Buffer for one vertical line (240 pixels * 2 bytes)

  for (;;)
  {
      current_time = HAL_GetTick();

      if ((current_time - last_beat_time) > ASYSTOLE_MS) {

          // Push '0' into the averaging buffer to drag the average down smoothly when no beats are detected
          bpm_buffer[bpm_idx] = 0;
          bpm_idx = (bpm_idx + 1) % BPM_BUFFER_SIZE;

          // Recalculate Global BPM
          uint16_t sum = 0;
          for(int i=0; i<BPM_BUFFER_SIZE; i++) {
              sum += bpm_buffer[i];
          }
          // Use full buffer size for division so average drops accurately
          global_bpm = sum / BPM_BUFFER_SIZE;

      }

    // 2. DATA PROCESSING
    if (xQueueReceive(ecgQueue, &val_from_queue, 1) == pdTRUE) {

      graph_average_sum += val_from_queue;
      graph_count++;

      hr_sum += val_from_queue;
      hr_count++;

      // Downsampling
      if (graph_count >= graph_downsample) {
          uint16_t val_to_draw = graph_average_sum / graph_downsample;
          graph_average_sum = 0;
          graph_count = 0;

        // --- DRAWING LOGIC (DMA) ---
        // Calculate Y position
        int32_t centered = (int32_t)val_to_draw - 2048;
        centered *= amplitude;
        int16_t y = 120 - (centered * 120 / 2048);

        // Safety Clamp (Keep Y between 51 and 239)
        if (y < 51) y = 51;
        if (y > 239) y = 239;

        // Erase Ahead of Graph
        uint16_t erase_x = x_pos + 5;
        if (erase_x >= 320) erase_x -= 320;

        if (xSemaphoreTake(lcdSpiSemaphore, portMAX_DELAY) == pdTRUE) {
           ILI_DrawVerticalLine(erase_x, 51, 239, 0x0000);
           xSemaphoreGive(lcdSpiSemaphore);
        }

        // 3. Prepare DMA Buffer (Green Pixels)
        uint16_t y1 = last_y_pos;
        uint16_t y2 = y;
        if (y1 > y2) { uint16_t temp = y1; y1 = y2; y2 = temp; }
        uint16_t height = y2 - y1 + 1;

        // DEBUG HEART BEAT CHECK

        if (beat_detected_debug_flag == 1) {
          if (hold_count > 0) {
            line_color = 0xF800; // RED
            hold_count--;
          }
          else {
            beat_detected_debug_flag = 0; // Reset flag
            line_color = 0x07E0; // Default Green
            hold_count = 10;
          }
        }

        for(int i=0; i < height * 2; i+=2) {
           dma_buffer[i]   = (line_color >> 8) & 0xFF; // High Byte (Green)
           dma_buffer[i+1] = line_color & 0xFF; // Low Byte (Green)
        }

        // 4. Send to Screen (Manual Address + DMA)
        if (xSemaphoreTake(lcdSpiSemaphore, portMAX_DELAY) == pdTRUE) {

           // --- Manual Address Window Setup ---
           // Column Address Set (X)
           ILI_Write(0x2A, 1);
           uint8_t x_d[] = {x_pos>>8, x_pos&0xFF, x_pos>>8, x_pos&0xFF};
           HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
           HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
           HAL_SPI_Transmit(&hspi1, x_d, 4, 10);
           HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

           // Page Address Set (Y)
           ILI_Write(0x2B, 1);
           uint8_t y_d[] = {y1>>8, y1&0xFF, y2>>8, y2&0xFF};
           HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
           HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);
           HAL_SPI_Transmit(&hspi1, y_d, 4, 10);
           HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);

           // Memory Write
           ILI_Write(0x2C, 1);
           HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
           HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
           // Tiny delay to ensure CS stable
           for(volatile int i=0; i<10; i++);
           HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

           // --- DMA Transfer ---
           // NOTE: We do NOT give the semaphore here. It is given in the ISR callback!
           if (HAL_SPI_Transmit_DMA(&hspi1, dma_buffer, height * 2) != HAL_OK) {
               // If DMA fails to start, we must release CS and Semaphore manually
               HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
               xSemaphoreGive(lcdSpiSemaphore);
           }
        }

        last_y_pos = y;
        x_pos++;
        if (x_pos >= 320) x_pos = 0;
      }

      int16_t result_bpm;

      // --- HR CALCULATION ---
      if (hr_count >= hr_downsample) {
        uint32_t raw_avg = hr_sum / hr_downsample;
        int16_t clean_val = ((int16_t)raw_avg - 2048) / 2; // Subtract 100 to compensate for noise
        hr_sum = 0;
        hr_count = 0;


        // Pipe the raw ADC value to the library
        int16_t latency_val = PT_StateMachine(clean_val);

        if (latency_val > 0) {
          result_bpm = PT_get_ShortTimeHR_output(200);
          global_bpm = result_bpm;
          last_beat_time = current_time;
          beat_detected_debug_flag = 1;
        }
      }
    }
  }
  /* USER CODE END StartDefaultTask */
}

/* USER CODE BEGIN Header_StartTask02 */
/**
* @brief Function implementing the sensorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask02 */
void StartTask02(void const * argument)
{
  /* USER CODE BEGIN StartTask02 */
  TickType_t xLastWakeTime;
  // 1ms = 1000Hz Sampling
  const TickType_t xFrequency = pdMS_TO_TICKS(1);
  xLastWakeTime = xTaskGetTickCount();

  for(;;)
  {
    HAL_ADC_PollForConversion(&hadc1, 2);
    uint16_t raw = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Start(&hadc1);

    // Minimal Noise Filter
    uint16_t filtered = Filter_Signal(raw);

    xQueueSend(ecgQueue, &filtered, 0);

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
  /* USER CODE END StartTask02 */
}

/* USER CODE BEGIN Header_StartTask03 */
/**
* @brief Function implementing the UITask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartTask03 */
void Safe_DrawString(uint16_t x, uint16_t y, char *str, uint16_t color, uint16_t bgcolor, FontDef *font) {
    while (*str) {
        if (xSemaphoreTake(lcdSpiSemaphore, portMAX_DELAY) == pdTRUE) {
            ILI_DrawChar(x, y, *str, color, bgcolor, font);
            xSemaphoreGive(lcdSpiSemaphore);
        }

        x += font->width;
        str++;
        osDelay(1);
    }
}

/* USER CODE END Header_StartUITask */
void StartTask03(void const * argument)
{
  /* USER CODE BEGIN StartUITask */
  char bpm_str[16];
  uint8_t last_drawn_bpm = 255;

  if (xSemaphoreTake(lcdSpiSemaphore, portMAX_DELAY) == pdTRUE) {
      ILI_Fill(0x0000);
      ILI_WriteString(10, 10, "ECG MONITOR", 0xFFFF, 0x0000, &Font_7x10);
      ILI_DrawHLine(0, 50, 320, 0x7BEF); // Header Line
      xSemaphoreGive(lcdSpiSemaphore);
  }

  for (;;)
  {
    osDelay(200);

    if (global_bpm != last_drawn_bpm) {
      if (global_bpm == 0) {
        Safe_DrawString(0, 30, "- 0 BPM     ", 0xF800, 0x0000, &Font_11x18);
      }
      else {
        sprintf(bpm_str, "- %3d BPM    ", global_bpm);
        Safe_DrawString(0, 30, bpm_str, 0xF800, 0x0000, &Font_11x18);
      }
      last_drawn_bpm = global_bpm;
    }
  }
  /* USER CODE END StartUITask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

// --- DMA COMPLETE CALLBACK ---
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
  if (hspi->Instance == SPI1) {
    // Deselect CS
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);

    // Wake up the Display Task
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xSemaphoreGiveFromISR(lcdSpiSemaphore, &xHigherPriorityTaskWoken);
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
  }
}

/* USER CODE END Application */
