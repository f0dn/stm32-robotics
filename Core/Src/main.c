#include "main.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

UART_HandleTypeDef huart2;

#define BNO055_ADDR         (0x28 << 1) // 7-bit address shifted for HAL
#define BNO055_ACC_DATA_X_LSB 0x08

extern I2C_HandleTypeDef hi2c1;

const uint16_t MOTOR_PINS[] = {GPIO_PIN_8,  GPIO_PIN_9,  GPIO_PIN_10,
                               GPIO_PIN_11, GPIO_PIN_12, GPIO_PIN_13,
                               GPIO_PIN_14, GPIO_PIN_15};

static char rx_buf[256];
static uint16_t rx_len = 0;

static void uart_send_str(const char *s) {
    HAL_UART_Transmit(&huart2, (uint8_t *)s, strlen(s), 200);
}

// Read 6 bytes from a vector register (e.g., accelerometer)
int bno055_read_vector(uint8_t reg, int16_t *x, int16_t *y, int16_t *z) {
    uint8_t buf[6];
    if (HAL_I2C_Mem_Read(&hi2c1, BNO055_ADDR, reg, I2C_MEMADD_SIZE_8BIT, buf, 6, 100) != HAL_OK)
        return 0;
    *x = (int16_t)(buf[0] | (buf[1] << 8));
    *y = (int16_t)(buf[2] | (buf[3] << 8));
    *z = (int16_t)(buf[4] | (buf[5] << 8));
    return 1;
}

/* Parse up to max_count positive integers from a comma-separated string.
   Returns 1 on success with exactly max_count numbers parsed, 0 otherwise. */
static int parse_csv_positive_ints(const char *s, int *out, int max_count) {
    char tmp[256];
    strncpy(tmp, s, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    char *saveptr = NULL;
    char *token = strtok_r(tmp, ",", &saveptr);
    int count = 0;
    while (token && count < max_count) {
        /* trim whitespace */
        char *start = token;
        while (*start && isspace((unsigned char)*start))
            start++;
        char *end = start + strlen(start) - 1;
        while (end > start && isspace((unsigned char)*end)) {
            *end = '\0';
            end--;
        }
        if (*start == '\0')
            return 0;
        char *endptr = NULL;
        long v = strtol(start, &endptr, 10);
        if (endptr == start || v < 0)
            return 0;
        out[count++] = (int)v;
        token = strtok_r(NULL, ",", &saveptr);
    }
    return (count == max_count) ? 1 : 0;
}

void SystemClock_Config(void);
static void init_all_motors_gpio(void);
static void MX_USART2_UART_Init(void);
static void dwt_delay_init(void);
static void delay_us(uint32_t us);
static void send_pulse_us(uint16_t us, uint16_t pin);

/* DWT-based microsecond delay helpers */
static void dwt_delay_init(void) {
    /* Enable DWT CYCCNT */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static void delay_us(uint32_t us) {
    uint32_t cycles = (SystemCoreClock / 1000000UL) * us;
    uint32_t start = DWT->CYCCNT;
    while ((DWT->CYCCNT - start) < cycles) { /* busy wait */
    }
}

static void send_pulse_us(uint16_t us, uint16_t pin) {
    if (us < 20)
        us = 20;
    if (us > 19980)
        us = 19980;
    HAL_GPIO_WritePin(MOTOR_GPIO_Port, pin, GPIO_PIN_SET);
    delay_us(us);
    HAL_GPIO_WritePin(MOTOR_GPIO_Port, pin, GPIO_PIN_RESET);
    delay_us(20000 - us);
}

/**
 * @brief The application entry point.
 * @retval int
 */
int main(void) {

    /* Reset of all peripherals, Initializes the Flash interface and the
     * Systick.
     */
    HAL_Init();

    /* Configure the system clock */
    SystemClock_Config();

    /* Initialize all configured peripherals */
    init_all_motors_gpio();
    MX_USART2_UART_Init();

    /* Simple microsecond delay init using DWT */
    dwt_delay_init();

    uart_send_str("Starting init\r\n");
    uart_send_str("Holding 1500us pulses\r\n");
    for (int i = 0; i < 100; ++i) {
        for (int j = 0; j < 8; ++j) {
            send_pulse_us(1500, MOTOR_PINS[j]);
        }
    }

    uart_send_str("Starting main code\r\n");
    while (1) {
        // for reading imu data
        // int16_t ax, ay, az;
        // bno055_read_vector(BNO055_ACC_DATA_X_LSB, &ax, &ay, &az);
        rx_len = 40;
        HAL_StatusTypeDef status =
            HAL_UART_Receive(&huart2, (uint8_t *)rx_buf, rx_len, 200);

        if (status == HAL_OK) {
            rx_buf[rx_len] = '\0';

            int values[8];
            if (parse_csv_positive_ints(rx_buf, values, 8)) {
                char resp[128];
                int n = snprintf(resp, sizeof(resp),
                                 "OK:%d,%d,%d,%d,%d,%d,%d,%d\r\n", values[0],
                                 values[1], values[2], values[3], values[4],
                                 values[5], values[6], values[7]);
                if (n > 0)
                    uart_send_str(resp);
            } else {
                uart_send_str(
                    "ERR: expected 8 positive integers comma-separated\r\n");
            }

            uart_send_str("First value: ");
            char val_str[16];
            uart_send_str(itoa(values[0], val_str, 10));
            uart_send_str("\r\n");

            for (int j = 0; j < 8; ++j) {
                send_pulse_us((uint16_t)values[j], MOTOR_PINS[j]);
            }

            rx_len = 0;
        }
    }
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
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
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |
                                  RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK) {
        Error_Handler();
    }
    PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USART2;
    PeriphClkInit.Usart2ClockSelection = RCC_USART2CLKSOURCE_PCLK1;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart2) != HAL_OK) {
        Error_Handler();
    }
}

static void init_all_motors_gpio(void) {
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    const size_t NUM_MOTORS = sizeof(MOTOR_PINS) / sizeof(MOTOR_PINS[0]);
    for (size_t motorIndex = 0; motorIndex < NUM_MOTORS; ++motorIndex) {
        const uint16_t motorPin = MOTOR_PINS[motorIndex];

        // configure pin as motor output
        GPIO_InitTypeDef gpioOptions = {
            .Pin = motorPin,
            .Mode = GPIO_MODE_OUTPUT_PP,
            .Pull = GPIO_NOPULL,
            .Speed = GPIO_SPEED_FREQ_LOW,
        };
        HAL_GPIO_Init(MOTOR_GPIO_Port, &gpioOptions);

        /* ensure output starts low */
        HAL_GPIO_WritePin(MOTOR_GPIO_Port, motorPin, GPIO_PIN_RESET);
    }
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state
     */
    __disable_irq();
    while (1) {
        uart_send_str("BIG BAD ERROR\r\n");
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
void assert_failed(uint8_t *file, uint32_t line) {
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line
       number, ex printf("Wrong parameters value: file %s on line %d\r\n", file,
       line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
