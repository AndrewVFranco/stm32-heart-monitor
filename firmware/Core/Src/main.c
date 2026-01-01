/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2025 STMicroelectronics.
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
#include "main.h"
#include "adc.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

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

/* USER CODE BEGIN PV */

/* USER CODE BEGIN PV */
#define FILTER_SIZE 8
#define SAMPLE_RATE_MS 2
volatile uint32_t raw_val = 0;
uint32_t last_tick = 0;
uint16_t filter_buffer[FILTER_SIZE] = {0}; // The circular buffer
uint8_t filter_index = 0;                  // Current position in the buffer
uint32_t filter_sum = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#include "stdio.h"
#include "string.h"
/* USER CODE BEGIN 0 */
// --- MINIMAL ILI9341 DRIVER ---
void ILI_Write(uint8_t data, uint8_t is_cmd) {
  // Set DC: 0 for Command, 1 for Data
  HAL_GPIO_WritePin(TFT_DC_GPIO_Port, TFT_DC_Pin, is_cmd ? GPIO_PIN_RESET : GPIO_PIN_SET);

  // Select Chip
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_RESET);

  // Transmit
  HAL_SPI_Transmit(&hspi1, &data, 1, 100);

  // Deselect Chip
  HAL_GPIO_WritePin(TFT_CS_GPIO_Port, TFT_CS_Pin, GPIO_PIN_SET);
}

void ILI_Init_Minimal() {
  // Hard Reset
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET); // RST LOW
  HAL_Delay(100);
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_SET);   // RST HIGH
  HAL_Delay(100);

  // Init Sequence (Forces all settings)
  ILI_Write(0x01, 1); HAL_Delay(1000); // Software Reset

  ILI_Write(0xCB, 1); ILI_Write(0x39, 0); ILI_Write(0x2C, 0); ILI_Write(0x00, 0); ILI_Write(0x34, 0); ILI_Write(0x02, 0); // Power Control A
  ILI_Write(0xCF, 1); ILI_Write(0x00, 0); ILI_Write(0xC1, 0); ILI_Write(0x30, 0); // Power Control B
  ILI_Write(0xE8, 1); ILI_Write(0x85, 0); ILI_Write(0x00, 0); ILI_Write(0x78, 0); // Driver Timing A
  ILI_Write(0xEA, 1); ILI_Write(0x00, 0); ILI_Write(0x00, 0); // Driver Timing B
  ILI_Write(0xED, 1); ILI_Write(0x64, 0); ILI_Write(0x03, 0); ILI_Write(0x12, 0); ILI_Write(0x81, 0); // Power On Sequence
  ILI_Write(0xF7, 1); ILI_Write(0x20, 0); // Pump Ratio

  ILI_Write(0xC0, 1); ILI_Write(0x23, 0); // Power Control 1
  ILI_Write(0xC1, 1); ILI_Write(0x10, 0); // Power Control 2
  ILI_Write(0xC5, 1); ILI_Write(0x3E, 0); ILI_Write(0x28, 0); // VCOM Control 1
  ILI_Write(0xC7, 1); ILI_Write(0x86, 0); // VCOM Control 2

  ILI_Write(0x36, 1); ILI_Write(0xE8, 0); // Memory Access Control (Rotation)
  ILI_Write(0x3A, 1); ILI_Write(0x55, 0); // Pixel Format (16-bit)

  ILI_Write(0xB1, 1); ILI_Write(0x00, 0); ILI_Write(0x18, 0); // Frame Rate Control
  ILI_Write(0xB6, 1); ILI_Write(0x08, 0); ILI_Write(0x82, 0); ILI_Write(0x27, 0); // Display Function Control

  ILI_Write(0x11, 1); HAL_Delay(120);  // Sleep Out
  ILI_Write(0x29, 1); HAL_Delay(100);  // Display On
}

void ILI_Fill(char data) {
  // 1. Set Column Address (0 to 239)
  ILI_Write(0x2A, 1); // CASET
  uint8_t col_data[] = {0x00, 0x00, 0x01, 0x3F}; // 0 start, 239 end
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); // Data
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // CS Low
  HAL_SPI_Transmit(&hspi1, col_data, 4, 100);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); // CS High

  // 2. Set Page Address (0 to 319)
  ILI_Write(0x2B, 1); // PASET
  uint8_t page_data[] = {0x00, 0x00, 0x00, 0xEF}; // 0 start, 319 end
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); // Data
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // CS Low
  HAL_SPI_Transmit(&hspi1, page_data, 4, 100);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); // CS High

  // 3. Start Memory Write
  ILI_Write(0x2C, 1); // RAMWR

  // 4. Send Red Data
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET); // Data
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // CS Low

  // We send 5 lines of red at a time to keep overhead low
  uint8_t line[20]; // Small buffer
  for(int i=0; i<20; i+=2) { line[i] = data; line[i+1] = 0x00; } // Fill buffer color

  // Total Pixels = 240 * 320 = 76,800
  // 76,800 pixels / 10 pixels per buffer = 7680 loops
  for(long i=0; i<7680; i++) {
    HAL_SPI_Transmit(&hspi1, line, 20, 10);
  }

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET); // CS High
}

// Helper to draw a single pixel
void ILI_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
  if(x >= 320 || y >= 240) return; // Error check

  // 1. Set Column Address (x to x)
  ILI_Write(0x2A, 1);
  uint8_t x_data[] = {x >> 8, x & 0xFF, x >> 8, x & 0xFF};
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);   // DC=Data
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // CS=Low
  HAL_SPI_Transmit(&hspi1, x_data, 4, 10);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // CS=High

  // 2. Set Page Address (y to y)
  ILI_Write(0x2B, 1);
  uint8_t y_data[] = {y >> 8, y & 0xFF, y >> 8, y & 0xFF};
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);   // DC=Data
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // CS=Low
  HAL_SPI_Transmit(&hspi1, y_data, 4, 10);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // CS=High

  // 3. Write Memory
  ILI_Write(0x2C, 1);
  uint8_t color_data[] = {color >> 8, color & 0xFF};
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);   // DC=Data
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // CS=Low
  HAL_SPI_Transmit(&hspi1, color_data, 2, 10);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // CS=High
}

// OPTIMIZED: Uses hardware windowing to draw lines 10x faster
void ILI_DrawVerticalLine(uint16_t x, uint16_t y1, uint16_t y2, uint16_t color) {
  // 1. Swap coordinates if needed so we always draw Top -> Bottom
  if (y1 > y2) {
    uint16_t temp = y1; y1 = y2; y2 = temp;
  }

  // 2. Clipping (prevent drawing off screen)
  if (x >= 320) return;
  if (y1 >= 240) y1 = 0;
  if (y2 >= 240) y2 = 319;

  uint16_t height = y2 - y1 + 1;

  // --- STEP 1: Define the Window (X, Y1 to Y2) ---

  // Set Column Address (X to X)
  ILI_Write(0x2A, 1);
  uint8_t x_data[] = {x >> 8, x & 0xFF, x >> 8, x & 0xFF};
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);   // DC = Data
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // CS = Low
  HAL_SPI_Transmit(&hspi1, x_data, 4, 10);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // CS = High

  // Set Page Address (Y1 to Y2) -- This is the key difference!
  ILI_Write(0x2B, 1);
  uint8_t y_data[] = {y1 >> 8, y1 & 0xFF, y2 >> 8, y2 & 0xFF};
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);   // DC = Data
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // CS = Low
  HAL_SPI_Transmit(&hspi1, y_data, 4, 10);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // CS = High

  // --- STEP 2: Blast the Color Data ---

  ILI_Write(0x2C, 1); // Memory Write Command

  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_SET);   // DC = Data
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET); // CS = Low manually

  // Create a small buffer of the color to send in chunks
  uint8_t high_byte = color >> 8;
  uint8_t low_byte = color & 0xFF;
  uint8_t buffer[20]; // Buffer for 10 pixels

  // Fill buffer with the target color
  for(int i=0; i<20; i+=2) {
      buffer[i] = high_byte;
      buffer[i+1] = low_byte;
  }

  // Send the pixels in batches
  int pixels_remaining = height;
  while(pixels_remaining > 0) {
     int pixels_to_send = (pixels_remaining > 10) ? 10 : pixels_remaining;
     HAL_SPI_Transmit(&hspi1, buffer, pixels_to_send * 2, 10);
     pixels_remaining -= pixels_to_send;
  }

  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);   // Release CS
}

uint16_t Filter_Signal(uint16_t new_val) {
  // Subtract the oldest value (at current index) from the sum
  filter_sum -= filter_buffer[filter_index];

  // Overwrite that spot with the NEW value
  filter_buffer[filter_index] = new_val;

  // Add the new value to the sum
  filter_sum += new_val;

  // Move the index forward
  filter_index++;

  // Wrap around if we hit the end of the buffer
  if (filter_index >= FILTER_SIZE) {
    filter_index = 0;
  }

  // 6. Return the average
  return filter_sum / FILTER_SIZE;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  /* USER CODE BEGIN 2 */
  ILI_Init_Minimal();
  ILI_Fill(0xF8);
  HAL_Delay(500);

  // Clear to Black so we can see the green line better
  ILI_Fill(0x0000);

  // Start the Heart Sensor
  HAL_ADC_Start(&hadc1);

  uint16_t x = 0;
  uint16_t last_y = 120;
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1) {

    if (HAL_GetTick() - last_tick >= SAMPLE_RATE_MS)
    {
      // Reset the timer
      last_tick += SAMPLE_RATE_MS;

      // Read ad8232 Sensor
      HAL_ADC_Start(&hadc1);
      HAL_ADC_PollForConversion(&hadc1, 10);
      raw_val = HAL_ADC_GetValue(&hadc1); // 0 to 4095

      // Filter the data
      uint16_t filtered_val = Filter_Signal(raw_val);
      // 1. Center the value (Subtract the midpoint ~2048)
      int32_t amplitude = (int32_t)filtered_val - 2048;

      // 2. Amplify! (Multiply by 3 for 3x zoom)
      // amplitude /= 2;

      // 3. Shift back to unsigned range
      int32_t amplitude_val = amplitude + 2048;

      // 4. Map to screen HEIGHT (0-320)
      // Note: We flip it (320 - val) because screen Y=0 is the top
      int16_t y = 240 - (amplitude_val * 240 / 4096);

      // 5. Safety Clamps (Crucial because Zoom might push it off screen!)
      if (y < 0) y = 0;
      if (y > 239) y = 239;

      // Clear a vertical bar 5 pixels ahead of the current X
      uint16_t erase_x = x + 5;
      if (erase_x >= 320) erase_x -= 320; // Wrap around if needed

      // Draw a full black line from top to bottom at the erase position
      ILI_DrawVerticalLine(erase_x, 0, 319, 0x0000);

      // Draw the signal
      ILI_DrawVerticalLine(x, last_y, y, 0x07E0); // 0x07E0 = Green

      // Advance drawing
      x++;

      if (x >= 320) {
        x = 0; // Wrap around to start
      }

      last_y = y;
      /* USER CODE END WHILE */
    }
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */
int _write(int file, char *ptr, int len)
{
	HAL_UART_Transmit(&huart2, (uint8_t*)ptr, len, HAL_MAX_DELAY);
	return len;
	}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
