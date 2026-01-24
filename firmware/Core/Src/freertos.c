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
#include "ad8232.h"
#include "graph.h"
#include "heart_rate.h"

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
volatile uint8_t beat_detected_debug_flag = 0;

// --- HARDWARE HANDLES (Defined in main.c) ---
extern ADC_HandleTypeDef hadc1;
extern SPI_HandleTypeDef hspi1;

// --- RTOS OBJECTS ---
QueueHandle_t ecgQueue;
SemaphoreHandle_t lcdSpiSemaphore;

/* USER CODE END Variables */
osThreadId graphTaskHandle;
osThreadId sensorTaskHandle;
osThreadId UITaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void Algorithm_Task(void const * argument);
void Data_Task(void const * argument);
void GUI_Task(void const * argument);

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
  osThreadDef(graphTask, Algorithm_Task, osPriorityNormal, 0, 512);
  graphTaskHandle = osThreadCreate(osThread(graphTask), NULL);

  /* definition and creation of sensorTask */
  osThreadDef(sensorTask, Data_Task, osPriorityRealtime, 0, 256);
  sensorTaskHandle = osThreadCreate(osThread(sensorTask), NULL);

  /* definition and creation of UITask */
  osThreadDef(UITask, GUI_Task, osPriorityLow, 0, 128);
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
void Algorithm_Task(void const * argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  PT_init();
  uint16_t val_from_queue;
  uint32_t current_time;

  for (;;)
  {
      current_time = HAL_GetTick();

    // --- DATA PROCESSING ---
    if (xQueueReceive(ecgQueue, &val_from_queue, 1) == pdTRUE) {

      Process_Graph(val_from_queue);
      Process_HeartRate(val_from_queue, current_time);

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
void Data_Task(void const * argument)
{
  /* USER CODE BEGIN StartTask02 */
  TickType_t xLastWakeTime = xTaskGetTickCount();

  // Define Sample Rate - 1ms = 1000Hz Sampling
  const TickType_t xFrequency = pdMS_TO_TICKS(1);

  // Debug to check dropped samples
  static uint32_t dropped_sample_count = 0;

  for(;;)

  {
    // --- Start Polling Sequence ---
    HAL_ADC_Start(&hadc1);

    // Check if hardware is active
    if (HAL_ADC_PollForConversion(&hadc1, 2) == HAL_OK) {

      // Read data from the HAL
      uint16_t raw = AD8232_Read();

      // Minimal Noise Filter (O(1))
      uint16_t filtered = Filter_Signal(raw);

      // Queue error handling
      if (xQueueSend(ecgQueue, &filtered, 0) != pdTRUE) {
        dropped_sample_count++;
      }
      else {
        // Hardware Failure Mode: ADC did not respond
        // feat: Add Hardware Error Message Here
      }
    }

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
/* USER CODE END Header_StartUITask */
void GUI_Task(void const * argument)
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
        ILI_Safe_WriteString(0, 30, "- 0 BPM     ", 0xF800, 0x0000, &Font_11x18);
      }
      else {
        sprintf(bpm_str, "- %3d BPM    ", global_bpm);
        ILI_Safe_WriteString(0, 30, bpm_str, 0xF800, 0x0000, &Font_11x18);
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
