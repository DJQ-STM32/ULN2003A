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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <string.h>

#include "driver_key.h"
#include "driver_led.h"
#include "driver_lcd.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
typedef StaticTask_t osStaticThreadDef_t;
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
/* 运行标志: 1 = 正在跑, 0 = 已停在原点. 全局, 主循环和电机延时都要用 */
volatile uint8_t g_run_flag = 0;

/* 停机请求: 1 = 按过按钮了, 这一轮跑完再停.
 *
 * 转动途中按下不立刻停机, 只把这里置 1; 等 main 里一轮 (逆180 + 顺180)
 * 走完、电机正好回到最原始的位置, 才真正断相停下。
 * 所以整轮期间只置位、不清零, 动作永远落在原点上。 */
volatile uint8_t g_stop_req = 0;

/* 断掉全部四相. 停机时用, 防电机堵转发热
 * (本轮最后停在 A|B 相, 正好是起始那一相, 也就是一个整步定位点,
 *  断电后转子不会偏, 原位就是这里) */
static void Motor_AllOff(void)
{
	HAL_GPIO_WritePin(MOTOR_PORT, MOTOR_ALL_PINS, GPIO_PIN_RESET);
}

/* 等按键松手.
 * 停机那一刻如果手还按着, 主循环的 Key_GetNum() 会立刻把这次按键当成
 * "再按一次启动", 电机刚停就又转起来。所以停机后先把这次按键放干净,
 * 这样必须重新按一次才会再次启动。 */
static void Key_WaitRelease(void)
{
	while (Key_Read() != 0)
	{
		HAL_Delay(10);
	}
}

/* 把指定的相置高, 其余三相全部关断, 然后延时.
 * 延时被切成 1ms 一小段, 每段采一次按键 —— 但只做标记, 不停。
 *
 * 这里必须用不打消抖的 Key_Read(), 不能用 Key_GetNum():
 * Key_GetNum() 里有 20ms 消抖 + 空等松手, 而这里的步进节拍只有 2ms,
 * 拿它轮询会把节拍整个卡住, 电机一顿一顿的。
 * 人手按一下至少几十毫秒, 1ms 采一次不会漏; 万一采到毛刺, 也只是提前
 * 一轮停, 不会乱动作。 */
static void Motor_Phase(uint16_t set_pins, uint16_t speed)
{
	HAL_GPIO_WritePin(MOTOR_PORT, set_pins, GPIO_PIN_SET);
	HAL_GPIO_WritePin(MOTOR_PORT, (uint16_t)(MOTOR_ALL_PINS & ~set_pins), GPIO_PIN_RESET);

	while (speed > 0)
	{
		if (g_stop_req == 0 && Key_Read() != 0)   /* 转动途中按下 -> 只记请求 */
		{
			g_stop_req = 1;
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

/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};
/* Definitions for LihgtSenorTask */
osThreadId_t LihgtSenorTaskHandle;
uint32_t LihgtSenorTaskBuffer[ 128 ];
osStaticThreadDef_t LihgtSenorTaskControlBlock;
const osThreadAttr_t LihgtSenorTask_attributes = {
  .name = "LihgtSenorTask",
  .cb_mem = &LihgtSenorTaskControlBlock,
  .cb_size = sizeof(LihgtSenorTaskControlBlock),
  .stack_mem = &LihgtSenorTaskBuffer[0],
  .stack_size = sizeof(LihgtSenorTaskBuffer),
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for RainSenorTask */
osThreadId_t RainSenorTaskHandle;
uint32_t RainSenorTaskBuffer[ 128 ];
osStaticThreadDef_t RainSenorTaskControlBlock;
const osThreadAttr_t RainSenorTask_attributes = {
  .name = "RainSenorTask",
  .cb_mem = &RainSenorTaskControlBlock,
  .cb_size = sizeof(RainSenorTaskControlBlock),
  .stack_mem = &RainSenorTaskBuffer[0],
  .stack_size = sizeof(RainSenorTaskBuffer),
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for MotorTask */
osThreadId_t MotorTaskHandle;
uint32_t MotorTaskBuffer[ 128 ];
osStaticThreadDef_t MotorTaskControlBlock;
const osThreadAttr_t MotorTask_attributes = {
  .name = "MotorTask",
  .cb_mem = &MotorTaskControlBlock,
  .cb_size = sizeof(MotorTaskControlBlock),
  .stack_mem = &MotorTaskBuffer[0],
  .stack_size = sizeof(MotorTaskBuffer),
  .priority = (osPriority_t) osPriorityLow,
};
/* Definitions for LCDTask */
osThreadId_t LCDTaskHandle;
uint32_t LCDTaskBuffer[ 128 ];
osStaticThreadDef_t LCDTaskControlBlock;
const osThreadAttr_t LCDTask_attributes = {
  .name = "LCDTask",
  .cb_mem = &LCDTaskControlBlock,
  .cb_size = sizeof(LCDTaskControlBlock),
  .stack_mem = &LCDTaskBuffer[0],
  .stack_size = sizeof(LCDTaskBuffer),
  .priority = (osPriority_t) osPriorityLow,
};
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
void StartDefaultTask(void *argument);
void LihgtSenor_Task(void *argument);
void RainSenor_Task(void *argument);
void Motor_Task(void *argument);
void LCD_Task(void *argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* ---------- 一轮: 逆时针 180, 再顺时针 180 ----------
 * 两次步数一样, 方向相反, 所以一轮跑完净位移为 0,
 * 电机必定回到出发的那一相, 也就是最原始的位置。
 * 4*64 = 256 次调用 = 180 度。 */
void High_MotorSpeed()
{
	for (uint32_t i = 0; i < 2*64; i++)
	{
	  Motor_two1(1);      /* 逆时针 180 */
	}
	for (uint32_t i = 0; i < 2*64; i++)
	{
	  Motor_two2(1);      /* 顺时针 180 */
	}

}

void Middle_MotorSpeed()
{
	for (uint32_t i = 0; i < 2*64; i++)
	{
	  Motor_two1(3);      /* 逆时针 180 */
	}
	for (uint32_t i = 0; i < 2*64; i++)
	{
	  Motor_two2(3);      /* 顺时针 180 */
	}

}

void Low_MotorSpeed()
{
	for (uint32_t i = 0; i < 2*64; i++)
	{
	  Motor_two1(5);      /* 逆时针 180 */
	}
	for (uint32_t i = 0; i < 2*64; i++)
	{
	  Motor_two2(5);      /* 顺时针 180 */
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
  MX_I2C1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  uint8_t S_len = 0;
  uint8_t K_len = 0;

  Key_Init();
  LCD_Init();
  LCD_Clear();

  LCD_PrintString(0, 0, "State: ");
  LCD_PrintString(0, 2, "Key: ");
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();

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
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* creation of LihgtSenorTask */
  LihgtSenorTaskHandle = osThreadNew(LihgtSenor_Task, NULL, &LihgtSenorTask_attributes);

  /* creation of RainSenorTask */
  RainSenorTaskHandle = osThreadNew(RainSenor_Task, NULL, &RainSenorTask_attributes);

  /* creation of MotorTask */
  MotorTaskHandle = osThreadNew(Motor_Task, NULL, &MotorTask_attributes);

  /* creation of LCDTask */
  LCDTaskHandle = osThreadNew(LCD_Task, NULL, &LCDTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

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

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN 5 */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_LihgtSenor_Task */
/**
* @brief Function implementing the LihgtSenorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LihgtSenor_Task */
void LihgtSenor_Task(void *argument)
{
  /* USER CODE BEGIN LihgtSenor_Task */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END LihgtSenor_Task */
}

/* USER CODE BEGIN Header_RainSenor_Task */
/**
* @brief Function implementing the RainSenorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_RainSenor_Task */
void RainSenor_Task(void *argument)
{
  /* USER CODE BEGIN RainSenor_Task */
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END RainSenor_Task */
}

/* USER CODE BEGIN Header_Motor_Task */
/**
* @brief Function implementing the MotorTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_Motor_Task */
void Motor_Task(void *argument)
{
  /* USER CODE BEGIN Motor_Task */
  /* Infinite loop */
  for(;;)
  {
	  if (g_run_flag == 1)
	 	  {

	 		  LCD_PrintStringClean(7, 0, "Start", &S_len);
	 		  LCD_PrintStringClean(5, 2, "ON", &K_len);

	 		  High_MotorSpeed();
	 		  Middle_MotorSpeed();
	 		  Low_MotorSpeed();

	 		  /* 一轮走完, 人已经在原点上, 这时候才处理停机请求 */
	 		  if (g_stop_req == 1)
	 		  {
	 			  LCD_PrintStringClean(7, 0, "Stop", &S_len);
	 			  LCD_PrintStringClean(5, 2, "OFF", &K_len);
	 			  Motor_AllOff();
	 			  g_run_flag = 0;
	 			  g_stop_req = 0;
	 			  Key_WaitRelease();   /* 把停机那次按键放干净, 免得它被当成启动 */
	 		  }
	 	  }
	 	  else
	 	  {
	 		  LCD_PrintStringClean(7, 0, "Stop", &S_len);
	 		  LCD_PrintStringClean(5, 2, "OFF", &K_len);

	 		  /* 停着的时候用 Key_GetNum(): 这里有 20ms 消抖并等松手,
	 		   * 一个干净的"按一下"才会启动, 不会因为抖动反复启停 */
	 		  if (Key_GetNum() == 1)
	 		  {
	 			  g_run_flag = 1;
	 		  }
	 	  }
    osDelay(1);
  }
  /* USER CODE END Motor_Task */
}

/* USER CODE BEGIN Header_LCD_Task */
/**
* @brief Function implementing the LCDTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_LCD_Task */
void LCD_Task(void *argument)
{
  /* USER CODE BEGIN LCD_Task */
	LCD_Init();
	LCD_Clear();
  /* Infinite loop */
  for(;;)
  {
    osDelay(1);
  }
  /* USER CODE END LCD_Task */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

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
