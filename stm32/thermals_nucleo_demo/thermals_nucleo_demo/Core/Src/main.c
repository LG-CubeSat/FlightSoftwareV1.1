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
#include "lcd1602.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#define LCD_EDITOR_ROWS     2U
#define LCD_EDITOR_COLUMNS  16U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

static uint8_t received_character;
static uint8_t lcd_row;
static uint8_t lcd_column;
static uint8_t editor_active;
static uint8_t escape_state;

static char lcd_buffer[LCD_EDITOR_ROWS][LCD_EDITOR_COLUMNS + 1U] =
{
  "                ",
  "                "
};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

static void editor_clear_buffer(void)
{
  uint8_t row;
  uint8_t column;

  for (row = 0U; row < LCD_EDITOR_ROWS; row++)
  {
    for (column = 0U; column < LCD_EDITOR_COLUMNS; column++)
    {
      lcd_buffer[row][column] = ' ';
    }

    lcd_buffer[row][LCD_EDITOR_COLUMNS] = '\0';
  }
}

static void editor_redraw(void)
{
  uint8_t row;

  for (row = 0U; row < LCD_EDITOR_ROWS; row++)
  {
    lcd_set_cursor(row, 0U);
    lcd_print(lcd_buffer[row]);
  }

  lcd_set_cursor(lcd_row, lcd_column);
}

static void editor_begin(void)
{
  if (editor_active == 0U)
  {
    editor_clear_buffer();
    lcd_clear();

    lcd_row = 0U;
    lcd_column = 0U;
    editor_active = 1U;

    editor_redraw();
  }
}

static void editor_clear(void)
{
  editor_clear_buffer();
  lcd_clear();

  lcd_row = 0U;
  lcd_column = 0U;
  editor_active = 1U;

  editor_redraw();
}

static void editor_write_character(char character)
{
  editor_begin();

  lcd_buffer[lcd_row][lcd_column] = character;

  if (lcd_column < (LCD_EDITOR_COLUMNS - 1U))
  {
    lcd_column++;
  }
  else
  {
    lcd_column = 0U;
    lcd_row = (lcd_row == 0U) ? 1U : 0U;
  }

  editor_redraw();
}

static void editor_backspace(void)
{
  if (editor_active == 0U)
  {
    return;
  }

  if (lcd_column > 0U)
  {
    lcd_column--;
  }
  else
  {
    lcd_row = (lcd_row == 0U) ? 1U : 0U;
    lcd_column = LCD_EDITOR_COLUMNS - 1U;
  }

  lcd_buffer[lcd_row][lcd_column] = ' ';
  editor_redraw();
}

static void editor_new_line(void)
{
  editor_begin();

  lcd_row = (lcd_row == 0U) ? 1U : 0U;
  lcd_column = 0U;

  editor_redraw();
}

static void editor_handle_arrow(uint8_t arrow)
{
  if (editor_active == 0U)
  {
    return;
  }

  if ((arrow == 'A') && (lcd_row > 0U))
  {
    lcd_row--;
  }
  else if ((arrow == 'B') && (lcd_row < (LCD_EDITOR_ROWS - 1U)))
  {
    lcd_row++;
  }
  else if ((arrow == 'C') && (lcd_column < (LCD_EDITOR_COLUMNS - 1U)))
  {
    lcd_column++;
  }
  else if ((arrow == 'D') && (lcd_column > 0U))
  {
    lcd_column--;
  }

  editor_redraw();
}

static void editor_process_byte(uint8_t byte)
{
  if (escape_state == 1U)
  {
    if ((byte == '[') || (byte == 'O'))
    {
      escape_state = 2U;
    }
    else
    {
      escape_state = 0U;
    }

    return;
  }

  if (escape_state == 2U)
  {
    if ((byte == 'A') || (byte == 'B') ||
        (byte == 'C') || (byte == 'D'))
    {
      editor_handle_arrow(byte);
    }
    else if (byte == 'Z')
    {
      /* Shift-Tab sends ESC [ Z. */
      editor_clear();
    }

    escape_state = 0U;
    return;
  }

  if (byte == 27U)
  {
    escape_state = 1U;
  }
  else if (byte == '\r')
  {
    editor_new_line();
  }
  else if (byte == '\n')
  {
    /* Ignore the second byte of a CR/LF line ending. */
  }
  else if ((byte == '\b') || (byte == 127U))
  {
    editor_backspace();
  }
  else if ((byte >= 32U) && (byte <= 126U))
  {
    editor_write_character((char)byte);
  }
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
  /* USER CODE BEGIN 2 */

  lcd_init();
  lcd_clear();

  lcd_set_cursor(0, 0);
  lcd_print("Type over USB:");

  lcd_row = 1U;
  lcd_column = 0U;
  editor_active = 0U;
  escape_state = 0U;
  lcd_set_cursor(lcd_row, lcd_column);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    if (HAL_UART_Receive(&huart2, &received_character, 1U, 10U) == HAL_OK)
    {
      editor_process_byte(received_character);
    }

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
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

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
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
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
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, KEYPAD_R4_Pin|KEYPAD_R1_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LCD_D6_Pin|LCD_E_Pin|LCD_D5_Pin|LCD_D4_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(KEYPAD_R2_GPIO_Port, KEYPAD_R2_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LCD_D7_Pin|LCD_RS_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(KEYPAD_R3_GPIO_Port, KEYPAD_R3_Pin, GPIO_PIN_SET);

  /*Configure GPIO pins : KEYPAD_C1_Pin KEYPAD_C2_Pin KEYPAD_C3_Pin */
  GPIO_InitStruct.Pin = KEYPAD_C1_Pin|KEYPAD_C2_Pin|KEYPAD_C3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : KEYPAD_R4_Pin LCD_D7_Pin KEYPAD_R1_Pin LCD_RS_Pin */
  GPIO_InitStruct.Pin = KEYPAD_R4_Pin|LCD_D7_Pin|KEYPAD_R1_Pin|LCD_RS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : KEYPAD_C4_Pin */
  GPIO_InitStruct.Pin = KEYPAD_C4_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(KEYPAD_C4_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LCD_D6_Pin LCD_E_Pin LCD_D5_Pin LCD_D4_Pin
                           KEYPAD_R3_Pin */
  GPIO_InitStruct.Pin = LCD_D6_Pin|LCD_E_Pin|LCD_D5_Pin|LCD_D4_Pin
                          |KEYPAD_R3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : KEYPAD_R2_Pin */
  GPIO_InitStruct.Pin = KEYPAD_R2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(KEYPAD_R2_GPIO_Port, &GPIO_InitStruct);

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
