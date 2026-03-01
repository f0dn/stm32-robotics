/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
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
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */


/* UART JSON receive buffer */
static char rx_buf[256];
static uint16_t rx_len = 0;

/* Simple helpers */
static void uart_send_str(const char *s)
{
  HAL_UART_Transmit(&huart2, (uint8_t*)s, strlen(s), 200);
}

/* Parse up to max_count positive integers from a comma-separated string.
   Returns 1 on success with exactly max_count numbers parsed, 0 otherwise. */
static int parse_csv_positive_ints(const char *s, int *out, int max_count)
{
  char tmp[256];
  strncpy(tmp, s, sizeof(tmp)-1);
  tmp[sizeof(tmp)-1] = '\0';

  char *saveptr = NULL;
  char *token = strtok_r(tmp, ",", &saveptr);
  int count = 0;
  while (token && count < max_count) {
    /* trim whitespace */
    char *start = token;
    while (*start && isspace((unsigned char)*start)) start++;
    char *end = start + strlen(start) - 1;
    while (end > start && isspace((unsigned char)*end)) { *end = '\0'; end--; }
    if (*start == '\0') return 0;
    char *endptr = NULL;
    long v = strtol(start, &endptr, 10);
    if (endptr == start || v < 0) return 0;
    out[count++] = (int)v;
    token = strtok_r(NULL, ",", &saveptr);
  }
  return (count == max_count) ? 1 : 0;
}

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void saved_main_loop(void);
static void dwt_delay_init(void);
static void delay_us(uint32_t us);
static void send_pulse_us(uint16_t us);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * Saved original main loop (moved here for now) */
static void saved_main_loop(void)
{
  while (1)
  {
    uint8_t ch;
    if (HAL_UART_Receive(&huart2, &ch, 1, 200) == HAL_OK)
    {
      if (rx_len < sizeof(rx_buf) - 1) rx_buf[rx_len++] = (char)ch;

      if (ch == '\n' || ch == '\r') {
        rx_buf[rx_len] = '\0';

        int values[8];
        if (parse_csv_positive_ints(rx_buf, values, 8)) {
          char resp[128];
          int n = snprintf(resp, sizeof(resp), "OK:%d,%d,%d,%d,%d,%d,%d,%d\n",
                           values[0],values[1],values[2],values[3],
                           values[4],values[5],values[6],values[7]);
          if (n > 0) uart_send_str(resp);
        } else {
          uart_send_str("ERR: expected 8 positive integers comma-separated\n");
        }

        rx_len = 0;
      }
    }
  }
}

/* DWT-based microsecond delay helpers */
static void dwt_delay_init(void)
{
  /* Enable DWT CYCCNT */
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us)
{
  uint32_t cycles = (SystemCoreClock / 1000000UL) * us;
  uint32_t start = DWT->CYCCNT;
  while ((DWT->CYCCNT - start) < cycles) { /* busy wait */ }
}

static void send_pulse_us(uint16_t us)
{
  if (us < 20) us = 20;
  if (us > 19980) us = 19980;
  HAL_GPIO_WritePin(MOTOR_GPIO_Port, MOTOR_Pin, GPIO_PIN_SET);
  delay_us(us);
  HAL_GPIO_WritePin(MOTOR_GPIO_Port, MOTOR_Pin, GPIO_PIN_RESET);
  delay_us(20000 - us);
}
/**
  * @brief  The application entry point.
  * @retval int
  */
void main1(void)
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
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Simple microsecond delay init using DWT */
  dwt_delay_init();

  /* Send 1500us pulses a few times to initialize motor controller/servo */
  for (int i = 0; i < 10; ++i) {
    send_pulse_us(1500);
  }

  /* Wait for user button (B1 on PC13) press to start the sweep.
     B1 is configured with internal pull-up, so pressed = pin pulled to GND (RESET). */
  uart_send_str("Waiting for button press to start sweep...\n");
  while (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_SET) {
    HAL_Delay(10);
  }
  /* simple debounce */
  HAL_Delay(50);
  /* wait for release so sweep doesn't immediately retrigger */
  while (HAL_GPIO_ReadPin(B1_GPIO_Port, B1_Pin) == GPIO_PIN_RESET) {
    HAL_Delay(10);
  }
  HAL_Delay(50);

  /* Sweep from 1100 to 1900 and back down */
  uart_send_str("Starting sweep\n");
  for (int v = 1100; v <= 1900; v += 10) {
    send_pulse_us((uint16_t)v);
  }
  for (int v = 1900; v >= 1100; v -= 10) {
    send_pulse_us((uint16_t)v);
  }

  uart_send_str("Sweep complete\n");

  /* After sweep, run the original main loop saved for now */
  saved_main_loop();
  /* USER CODE END 3 */
}


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
  /* USER CODE BEGIN 2 */

  /* USER CODE END 2 */

  /* Simple microsecond delay init using DWT */
  dwt_delay_init();


  //delay_us(1000000);

  uart_send_str("Holding 1500us pulses\n");
  for(int i = 0; i < 100; ++i) {
    send_pulse_us(1500); /* send_pulse_us already yields a 20ms frame (high for 1500us, low for rest) */
  }

  //send_pulse_us(1500);
  uart_send_str("Starting init\n");

  //delay_us(5000000);


  /* Send 1500us pulses a few times to initialize motor controller/servo */
  uart_send_str("Starting sweep\n");
  send_pulse_us(1900);
  //delay_us(5000000);
  while (1) {
    for (int v = 1100; v <= 1900; v += 10) {
      send_pulse_us((uint16_t)v);
    }
    delay_us(500000);
    for (int v = 1900; v >= 1100; v -= 10) {
      send_pulse_us((uint16_t)v);
    }
  }
  uart_send_str("Ending sweep\n");


  /* After sweep, run the original main loop saved for now */
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
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
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
  PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 38400;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure MOTOR signal pin Output Level */
  HAL_GPIO_WritePin(MOTOR_GPIO_Port, MOTOR_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  /* Enable internal pull-up so button can be wired to GND when pressed */
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : MOTOR_Pin (servo/motor signal) */
  GPIO_InitStruct.Pin = MOTOR_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(MOTOR_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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
