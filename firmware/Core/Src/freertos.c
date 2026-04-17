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
#include "ad8232.h"
#include "graph.h"
#include "heart_rate.h"
#include "PanTompkins.h"
#include "tim.h"
#include "inference.h"

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

// ECG window
#define INFERENCE_INTERVAL 150
float    ecg_window[ECG_WINDOW_SIZE];
uint16_t window_idx       = 0;
uint16_t samples_since_inference = 0;
uint8_t  window_primed    = 0;

volatile RhythmClass_t global_rhythm   = RHYTHM_NONE;
volatile float         global_confidence = 0.0f;

// --- SHARED DATA ---
volatile uint8_t global_bpm = 0;

// HR Debug
// volatile uint8_t beat_detected_debug_flag = 0;

// --- HARDWARE HANDLES (Defined in main.c) ---
extern ADC_HandleTypeDef hadc1;
extern SPI_HandleTypeDef hspi1;

// --- RTOS OBJECTS ---
QueueHandle_t ecgQueue;
SemaphoreHandle_t lcdSpiSemaphore;
SemaphoreHandle_t inferenceReadySemaphore;

/* USER CODE END Variables */
osThreadId graphTaskHandle;
osThreadId sensorTaskHandle;
osThreadId UITaskHandle;
osThreadId inferenceTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartGraphTask(void const * argument);
void StartSensorTask(void const * argument);
void StartUITask(void const * argument);
void StartInferenceTask(void const * argument);

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

  // Create Semaphores (Binary)
  lcdSpiSemaphore = xSemaphoreCreateBinary();
  xSemaphoreGive(lcdSpiSemaphore);

  inferenceReadySemaphore = xSemaphoreCreateBinary();

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
  osThreadDef(graphTask, StartGraphTask, osPriorityNormal, 0, 512);
  graphTaskHandle = osThreadCreate(osThread(graphTask), NULL);

  /* definition and creation of sensorTask */
  osThreadDef(sensorTask, StartSensorTask, osPriorityRealtime, 0, 256);
  sensorTaskHandle = osThreadCreate(osThread(sensorTask), NULL);

  /* definition and creation of UITask */
  osThreadDef(UITask, StartUITask, osPriorityLow, 0, 128);
  UITaskHandle = osThreadCreate(osThread(UITask), NULL);

  /* definition and creation of inferenceTask */
  osThreadDef(inferenceTask, StartInferenceTask, osPriorityIdle, 0, 1024);
  inferenceTaskHandle = osThreadCreate(osThread(inferenceTask), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

}

/* USER CODE BEGIN Header_StartGraphTask */
/**
  * @brief  Function implementing the graphTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartGraphTask */
void StartGraphTask(void const * argument)
{
  /* USER CODE BEGIN StartGraphTask */
  PT_init();
  uint16_t val_from_queue;
  uint16_t filtered_val;
  uint32_t current_time;
  uint8_t decimation_counter = 0;

  /* Infinite loop */
  for(;;)
  {
    current_time = HAL_GetTick();

    // --- DATA PROCESSING ---
    if (xQueueReceive(ecgQueue, &val_from_queue, 1) == pdTRUE) {

      filtered_val = Filter_Signal(val_from_queue);

      // Keep UI and Heart Rate running at 1kHz for accuracy/smoothness
      Process_Graph(filtered_val);
      Process_HeartRate(filtered_val, current_time);

      // --- DOWNSAMPLING LOGIC (1kHz -> 100Hz) ---
      decimation_counter++;
      if (decimation_counter >= 10) {
        decimation_counter = 0; // Reset counter

        // Write into the AI circular buffer (happens 100 times a second)
        ecg_window[window_idx] = ((float)filtered_val - 2048.0f) / 2048.0f;
        window_idx = (window_idx + 1) % ECG_WINDOW_SIZE;

        // Prime check — wait for first full 10-second window before inferring
        if (!window_primed) {
          if (window_idx == 0) window_primed = 1;
          continue;
        }

        // Trigger inference every INFERENCE_INTERVAL samples
        // (150 samples at 100Hz = triggers every 1.5 seconds)
        samples_since_inference++;
        if (samples_since_inference >= INFERENCE_INTERVAL) {
          samples_since_inference = 0;
          xSemaphoreGive(inferenceReadySemaphore);
        }
      }
    }
  }
  /* USER CODE END StartGraphTask */
}

/* USER CODE BEGIN Header_StartSensorTask */
/**
* @brief Function implementing the sensorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartSensorTask */
void StartSensorTask(void const * argument)
{
  /* USER CODE BEGIN StartSensorTask */
    HAL_ADC_Start_IT(&hadc1);
    HAL_TIM_Base_Start(&htim2);
  /* Infinite loop */
  for(;;)
  {
    osDelay(1000);
  }
  /* USER CODE END StartSensorTask */
}

/* USER CODE BEGIN Header_StartUITask */
/**
* @brief Function implementing the UITask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartUITask */
void StartUITask(void const * argument)
{
  /* USER CODE BEGIN StartUITask */
  char bpm_str[16];
  char conf_str[16];
  uint8_t last_drawn_bpm = 255;
  float last_drawn_conf = 0;

  if (xSemaphoreTake(lcdSpiSemaphore, portMAX_DELAY) == pdTRUE) {
    ILI_Fill(0x0000);
    ILI_WriteString(10, 10, "ECG MONITOR", 0xFFFF, 0x0000, &Font_7x10);
    ILI_WriteString(190, 10, "RHYTHM ANALYSIS", 0xFFFF, 0x0000, &Font_7x10);
    ILI_DrawHLine(0, 50, 320, 0x7BEF); // Header Line
    xSemaphoreGive(lcdSpiSemaphore);
  }

  /* Infinite loop */
  for(;;)
  {
    osDelay(200);

    if (global_bpm != last_drawn_bpm) {
      if (global_bpm == 0) {
        ILI_Safe_WriteString(0, 30, "- 0 BPM     ", 0xF800, 0x0000, &Font_11x18);
      }
      else {
        sprintf(bpm_str, "+ %3d BPM    ", global_bpm);
        ILI_Safe_WriteString(0, 30, bpm_str, 0xF800, 0x0000, &Font_11x18);
      }
      last_drawn_bpm = global_bpm;
    }

    if (global_confidence != last_drawn_conf) {
      if (global_confidence > .75) {
        ILI_Safe_WriteString(200, 30, (char*)get_rhythm_string(global_rhythm),0xF800, 0x0000, &Font_7x10);
        sprintf(conf_str, "%3d%%", (int)(global_confidence * 100.0f));
        ILI_Safe_WriteString(160, 30, conf_str, 0xF800, 0x0000, &Font_7x10);
      }
      else {
        ILI_Safe_WriteString(200, 30, "UNSURE - CHECK",0xF800, 0x0000, &Font_7x10);
        sprintf(conf_str, "%3d%%", (int)(global_confidence * 100.0f));
        ILI_Safe_WriteString(160, 30, conf_str, 0xF800, 0x0000, &Font_7x10);
      }
    }
    last_drawn_conf = global_confidence;

  }
  /* USER CODE END StartUITask */
}

/* USER CODE BEGIN Header_StartInferenceTask */
/**
* @brief Function implementing the inferenceTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartInferenceTask */
__weak void StartInferenceTask(void const * argument)
{
  /* USER CODE BEGIN StartInferenceTask */
  if (!inference_init()) {
    while(1) { osDelay(1000); }
  }

  /* Infinite loop */
  for(;;)
  {
    if (xSemaphoreTake(inferenceReadySemaphore, portMAX_DELAY) == pdTRUE) {
      global_rhythm = inference_run((float*)ecg_window, window_idx, (float *)&global_confidence);
    }
  }
  /* USER CODE END StartInferenceTask */
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
