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
typedef enum {
  STATE_BOOT,
  STATE_NORMAL,
  STATE_ASYSTOLE
} MonitorState_t;

// --- SHARED DATA ---
volatile uint8_t global_bpm = 0;
volatile MonitorState_t global_state = STATE_BOOT;

// --- PAN-TOMPKINS SETTINGS ---
#define WINDOW_SIZE 30
#define MAX_SLOPE 150
#define SLOPE_THRESHOLD 20000
#define MIN_PEAK_AMPLITUDE 1000
#define ASYSTOLE_MS 4000


volatile uint8_t amplitude = 1.5;

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
uint16_t val_from_queue;

  // --- SIGNAL PROCESSING VARIABLES ---
  uint32_t sample_average_sum = 0;
  uint8_t sample_count = 0;
  const uint8_t samples_per_pixel = 7;
  const float amplitude = 1.0f;

  // --- PAN-TOMPKINS VARIABLES ---
  #define WINDOW_SIZE 30
  #define MAX_SLOPE 150
  #define SLOPE_THRESHOLD 20000

  static uint32_t integration_buffer[WINDOW_SIZE] = {0};
  static uint8_t window_ptr = 0;
  static uint32_t running_sum = 0;
  static uint16_t prev_raw = 2048;

  // --- BEAT DETECTION STATE MACHINE ---
  typedef enum { BEAT_IDLE, BEAT_RISING, BEAT_COOLDOWN } BeatState_t;
  BeatState_t beat_state = BEAT_IDLE;
  uint32_t last_beat_time = 0;
  uint32_t peak_value = 0;

  // --- HR AVERAGING ---
  #define BPM_BUFFER_SIZE 5
  uint32_t bpm_buffer[BPM_BUFFER_SIZE] = {0};
  uint8_t bpm_ptr = 0;

  // --- DISPLAY VARIABLES ---
  static uint8_t dma_buffer[480]; // Buffer for one vertical line (240 pixels * 2 bytes)
  uint16_t x_pos = 0;
  uint16_t last_y_pos = 120;

  // --- INITIAL SETUP ---
  ILI_Fill(0x0000);
  ILI_DrawHLine(0, 50, 320, 0x7BEF); // Header Line

  for (;;)
  {
    // 1. ASYSTOLE CHECK (Safety Timeout)
    if ((HAL_GetTick() - last_beat_time) > ASYSTOLE_MS) {
      global_state = STATE_ASYSTOLE;
      global_bpm = 0;

      beat_state = BEAT_IDLE;
    }

    // 2. DATA PROCESSING
    // We wait 10ms for data. If empty, we loop back.
    if (xQueueReceive(ecgQueue, &val_from_queue, 10) == pdTRUE) {

      sample_average_sum += val_from_queue;
      sample_count++;

      // Downsampling: Only process 1 pixel for every 7 samples
      if (sample_count >= samples_per_pixel) {

        uint16_t val_to_draw = sample_average_sum / samples_per_pixel;
        sample_average_sum = 0;
        sample_count = 0;

        // --- PAN-TOMPKINS ALGORITHM ---
        int16_t slope = (int16_t)val_to_draw - prev_raw;
        prev_raw = val_to_draw;

        // Clamp slope
        if (slope > MAX_SLOPE) slope = MAX_SLOPE;
        if (slope < -MAX_SLOPE) slope = -MAX_SLOPE;

        int32_t squared_slope = (int32_t)slope * slope;

        running_sum -= integration_buffer[window_ptr];
        integration_buffer[window_ptr] = squared_slope;
        running_sum += squared_slope;

        window_ptr++;
        if (window_ptr >= WINDOW_SIZE) window_ptr = 0;

        // --- BEAT DETECTION STATE MACHINE ---
        uint32_t now = HAL_GetTick();

        switch (beat_state) {
          case BEAT_IDLE:
             // Wait for signal to rise above the noise floor
             if (running_sum > SLOPE_THRESHOLD) {
                 beat_state = BEAT_RISING;
                 peak_value = running_sum;
             }
             break;

            case BEAT_RISING:
                if (running_sum > peak_value) peak_value = running_sum;

                if (running_sum < (peak_value / 2)) {

                    // 1. Check Noise
                    if (peak_value < MIN_PEAK_AMPLITUDE) {
                        beat_state = BEAT_IDLE;
                        break;
                    }

                    // 2. Valid Beat confirmed
                    uint32_t diff = now - last_beat_time;

                    // 3. Reset the "Stopwatch" for Asystole
                    // Crucial: Update this even if BPM is weird, so we don't go to 0
                    last_beat_time = now;

                    if (diff > 250) {
                        // ... Calculate BPM ...
                    }
                    beat_state = BEAT_COOLDOWN;
                }
                break;

          case BEAT_COOLDOWN:
             // Wait for signal to return to baseline before looking for next beat
             if (running_sum < (SLOPE_THRESHOLD / 2)) {
                 beat_state = BEAT_IDLE;
             }
             // Timeout safety: if signal stays high for >150ms, force reset
             if (now - last_beat_time > 150) {
                 beat_state = BEAT_IDLE;
             }
             break;
        }

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

        for(int i=0; i < height * 2; i+=2) {
           dma_buffer[i]   = 0x07; // High Byte (Green)
           dma_buffer[i+1] = 0xE0; // Low Byte (Green)
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
        // Grab the bus just for ONE character
        if (xSemaphoreTake(lcdSpiSemaphore, portMAX_DELAY) == pdTRUE) {
            ILI_DrawChar(x, y, *str, color, bgcolor, font);
            xSemaphoreGive(lcdSpiSemaphore);
        }

        // Move X position for the next character
        x += font->width;

        // Move to next character in the string
        str++;

        // Force a context switch to let the Graph Task run if it needs to
        osDelay(1);
    }
}

/* USER CODE END Header_StartUITask */
void StartTask03(void const * argument)
{
  /* USER CODE BEGIN StartUITask */
  char bpm_str[16];
  uint8_t old_bpm = 255;
  MonitorState_t old_state = STATE_BOOT;


  if (xSemaphoreTake(lcdSpiSemaphore, portMAX_DELAY) == pdTRUE) {
      ILI_WriteString(10, 10, "ECG MONITOR", 0xFFFF, 0x0000, &Font_7x10);
      ILI_Fill(0x0000);
      ILI_DrawHLine(0, 50, 320, 0x7BEF); // Header Line
      xSemaphoreGive(lcdSpiSemaphore);
  }

  for (;;)
  {
    osDelay(250);

    if (global_bpm != old_bpm || global_state != old_state) {

        if (global_state == STATE_ASYSTOLE) {
            Safe_DrawString(10, 30, "ASYSTOLE  ", 0xF800, 0x0000, &Font_11x18);
        }
        else {
            sprintf(bpm_str, "%3d BPM +   ", global_bpm);
            Safe_DrawString(-10, 30, bpm_str, 0xF800, 0x0000, &Font_11x18);
        }

        old_bpm = global_bpm;
        old_state = global_state;

        xSemaphoreGive(lcdSpiSemaphore);
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
