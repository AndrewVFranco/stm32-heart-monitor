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
osThreadId displayTaskHandle;
osThreadId sensorTaskHandle;

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void const * argument);
void StartTask02(void const * argument);

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
  /* definition and creation of displayTask */
  osThreadDef(displayTask, StartDefaultTask, osPriorityNormal, 0, 512);
  displayTaskHandle = osThreadCreate(osThread(displayTask), NULL);

  /* definition and creation of sensorTask */
  osThreadDef(sensorTask, StartTask02, osPriorityRealtime, 0, 256);
  sensorTaskHandle = osThreadCreate(osThread(sensorTask), NULL);

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
uint16_t val_from_queue;

  // Downsampling State
  uint32_t averaging_sum = 0;
  uint8_t sample_count = 0;
  const uint8_t SAMPLES_PER_PIXEL = 7; // 1000Hz / 7 = ~142Hz (25mm/s)

  // DMA Buffer
  static uint8_t dma_buffer[480];

  for(;;)
  {
    // Wait for Data (Indefinitely)
    if (xQueueReceive(ecgQueue, &val_from_queue, portMAX_DELAY) == pdTRUE)
    {
       // 1. Accumulate Data (Downsampling Logic)
       averaging_sum += val_from_queue;
       sample_count++;

       // 2. Only Draw when we have enough samples for 1 Pixel
       if (sample_count >= SAMPLES_PER_PIXEL)
       {
           // Calculate Average
           uint16_t val_to_draw = averaging_sum / SAMPLES_PER_PIXEL;

           // Reset Accumulators
           averaging_sum = 0;
           sample_count = 0;

           // --- DRAWING LOGIC STARTS HERE ---

           // Vertical Zoom
           int32_t centered = (int32_t)val_to_draw - 2048;
           centered *= amplitude;
           int16_t y = 120 - (centered * 120 / 2048);
           if (y < 0) y = 0; if (y > 239) y = 239;

           // Erase Bar
           uint16_t erase_x = x_pos + 5;
           if (erase_x >= 320) erase_x -= 320;

           if (xSemaphoreTake(lcdSpiSemaphore, portMAX_DELAY) == pdTRUE) {
               ILI_DrawVerticalLine(erase_x, 0, 239, 0x0000);
               xSemaphoreGive(lcdSpiSemaphore);
           }

           // Prepare Buffer
           uint16_t y1 = last_y_pos;
           uint16_t y2 = y;
           if (y1 > y2) { uint16_t temp = y1; y1 = y2; y2 = temp; }
           uint16_t height = y2 - y1 + 1;

           for(int i=0; i < height * 2; i+=2) {
               dma_buffer[i]   = 0x07;
               dma_buffer[i+1] = 0xE0;
           }

           // DMA Transfer
           if (xSemaphoreTake(lcdSpiSemaphore, portMAX_DELAY) == pdTRUE) {

               // Window Setup
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

               // Safety Reset & Launch
               HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, GPIO_PIN_SET);
               HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
               for(volatile int i=0; i<10; i++);
               HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

               if (HAL_SPI_Transmit_DMA(&hspi1, dma_buffer, height * 2) != HAL_OK) {
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

    // Minimal Noise Filter (Keep this, it's good for everyone)
    uint16_t filtered = Filter_Signal(raw);

    // Send RAW data.
    // We wait 0 ticks. If Queue is full, we drop data (Queue Overflow).
    xQueueSend(ecgQueue, &filtered, 0);

    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
  /* USER CODE END StartTask02 */
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
