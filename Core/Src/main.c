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

#include "driver_key.h"
#include "driver_led.h"
#include "driver_lcd.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* ---- 步进电机引脚映射 ----------------------------------------------
 * 2026-10-09: 原来的 PA3/PA4/PA5/PA6 中, PA4 和 PA6 被 12V 反向灌入打坏
 * (当时 GPIO 被直接接在电机相线上, 而相线连着 ULN2003 输出)。
 * 现改用 PB0/PB1/PB8/PB9, 这 4 个脚在本工程中完全空闲。
 *
 *   接线:  PB0 -> 驱动板 IN1
 *          PB1 -> 驱动板 IN2
 *          PB8 -> 驱动板 IN3
 *          PB9 -> 驱动板 IN4
 * ------------------------------------------------------------------- */
#define MOTOR_PORT     GPIOB
#define MOTOR_A_PIN    GPIO_PIN_0      /* -> ULN2003 IN1 */
#define MOTOR_B_PIN    GPIO_PIN_1      /* -> ULN2003 IN2 */
#define MOTOR_C_PIN    GPIO_PIN_8      /* -> ULN2003 IN3 */
#define MOTOR_D_PIN    GPIO_PIN_9      /* -> ULN2003 IN4 */

#define MOTOR_ALL_PINS (MOTOR_A_PIN | MOTOR_B_PIN | MOTOR_C_PIN | MOTOR_D_PIN)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* 运行标志: 1 = 运行, 0 = 暂停. 全局, 主循环和电机延时都要用 */
volatile uint8_t g_run_flag = 0;

/* 记录当前正在通电的相, 暂停/恢复时要重新打开它 */
static uint16_t g_current_phase = 0;

/* 暂停: 断掉所有相, 死等按键恢复, 恢复后把当前相重新打开 */
static void Motor_WaitResume(void)
{
	HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_ALL_PINS, GPIO_PIN_RESET);   /* 断电, 防发热 */

	while (g_run_flag == 0)
	{
		if (Key_GetNum() == 1)
		{
			g_run_flag = 1;
		}
		HAL_Delay(1);
	}

	/* 恢复: 重新打开暂停前的那一相 */
	HAL_GPIO_WritePin(MOTOR_PORT, g_current_phase, GPIO_PIN_SET);
	HAL_GPIO_WritePin(MOTOR_PORT,
	                  (uint16_t)(MOTOR_ALL_PINS & ~g_current_phase),
	                  GPIO_PIN_RESET);
}

/* 把指定的相置高, 其余三相全部关断, 然后延时.
 * 延时被切成 1ms 一小段, 每段检测一次按键, 这样转动途中也能暂停/恢复 */
static void Motor_Phase(uint16_t set_pins, uint16_t speed)
{
	g_current_phase = set_pins;

	/* 如果进来时就已经是暂停状态, 先等 */
	if (g_run_flag == 0)
	{
		Motor_WaitResume();
	}

	HAL_GPIO_WritePin(MOTOR_PORT, set_pins, GPIO_PIN_SET);
	HAL_GPIO_WritePin(MOTOR_PORT, (uint16_t)(MOTOR_ALL_PINS & ~set_pins), GPIO_PIN_RESET);

	while (speed > 0)
	{
		if (Key_GetNum() == 1)          /* 转动途中按下 -> 翻转状态 */
		{
			g_run_flag = !g_run_flag;
			if (g_run_flag == 0)
			{
				Motor_WaitResume();     /* 暂停并等待恢复 */
			}
		}
		HAL_Delay(1);
		speed--;
	}
}


void Motor_one1(uint16_t speed)		//1相励磁 A-B-C-D
{
	Motor_Phase(MOTOR_A_PIN, speed);
	Motor_Phase(MOTOR_B_PIN, speed);
	Motor_Phase(MOTOR_C_PIN, speed);
	Motor_Phase(MOTOR_D_PIN, speed);
}

void Motor_two1(uint16_t speed)		//2相励磁 AB-BC-CD-DA
{
	Motor_Phase(MOTOR_A_PIN | MOTOR_B_PIN, speed);
	Motor_Phase(MOTOR_B_PIN | MOTOR_C_PIN, speed);
	Motor_Phase(MOTOR_C_PIN | MOTOR_D_PIN, speed);
	Motor_Phase(MOTOR_D_PIN | MOTOR_A_PIN, speed);
}

void Motor_one_two1(uint16_t speed)	//1-2相励磁(半步) A-AB-B-BC-C-CD-D-DA
{
	Motor_Phase(MOTOR_A_PIN, speed);
	Motor_Phase(MOTOR_A_PIN | MOTOR_B_PIN, speed);
	Motor_Phase(MOTOR_B_PIN, speed);
	Motor_Phase(MOTOR_B_PIN | MOTOR_C_PIN, speed);
	Motor_Phase(MOTOR_C_PIN, speed);
	Motor_Phase(MOTOR_C_PIN | MOTOR_D_PIN, speed);
	Motor_Phase(MOTOR_D_PIN, speed);
	Motor_Phase(MOTOR_D_PIN | MOTOR_A_PIN, speed);
}

void Motor_one2(uint16_t speed)		//1相励磁 A-B-C-D
{
	Motor_Phase(MOTOR_D_PIN, speed);
	Motor_Phase(MOTOR_C_PIN, speed);
	Motor_Phase(MOTOR_B_PIN, speed);
	Motor_Phase(MOTOR_A_PIN, speed);
}

void Motor_two2(uint16_t speed)		//2相励磁 AB-BC-CD-DA
{
	Motor_Phase(MOTOR_D_PIN | MOTOR_A_PIN, speed);
	Motor_Phase(MOTOR_C_PIN | MOTOR_D_PIN, speed);
	Motor_Phase(MOTOR_B_PIN | MOTOR_C_PIN, speed);
	Motor_Phase(MOTOR_A_PIN | MOTOR_B_PIN, speed);
}

void Motor_one_two2(uint16_t speed)	//1-2相励磁(半步) A-AB-B-BC-C-CD-D-DA
{
	Motor_Phase(MOTOR_D_PIN, speed);
	Motor_Phase(MOTOR_D_PIN | MOTOR_A_PIN, speed);
	Motor_Phase(MOTOR_C_PIN, speed);
	Motor_Phase(MOTOR_C_PIN | MOTOR_D_PIN, speed);
	Motor_Phase(MOTOR_B_PIN, speed);
	Motor_Phase(MOTOR_B_PIN | MOTOR_C_PIN, speed);
	Motor_Phase(MOTOR_A_PIN, speed);
	Motor_Phase(MOTOR_A_PIN | MOTOR_B_PIN, speed);
}

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


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
  MX_I2C1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  uint8_t KeyNum = 0;

  Key_Init();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  KeyNum = Key_GetNum();
	  if (KeyNum == 1)
	  {
		  g_run_flag = !g_run_flag;
	  }
	  if (g_run_flag == 1)
	  {
		  for (uint32_t i = 0; i <= 4*64; i++)
		  {
			  Motor_one1(2);      /* 连续转动, 512 次调用 = 一整圈 */
		  }
		  for (uint32_t i = 0; i <= 4*64; i++)
		  {
			  Motor_one2(2);      /* 连续转动, 512 次调用 = 一整圈 */
		  }
	  }

    /* USER CODE END WHILE */

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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

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
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5|GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin : PC13 */
  GPIO_InitStruct.Pin = GPIO_PIN_13;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pins : PA1 PA2 PA3 PA4
                           PA5 PA6 */
  GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* 步进电机 4 相输出脚 PB0/PB1/PB8/PB9.
   * 放在 USER CODE 块里, 这样以后用 CubeMX 重新生成代码也不会被覆盖。
   * (GPIOB 的时钟在上面已经使能了) */
  HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_ALL_PINS, GPIO_PIN_RESET);

  GPIO_InitStruct.Pin   = MOTOR_ALL_PINS;
  GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull  = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(MOTOR_PORT, &GPIO_InitStruct);

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
