/*
 * driver_key.c
 *
 *  Created on: Aug 20, 2026
 *      Author: 杜家强
 */

#include "driver_key.h"
#include "driver_led.h"
#include "driver_lcd.h"


#include "main.h"

#define KEY_GPIO_GROUP GPIOB
#define KEY_GPIO_PIN   GPIO_PIN_14

void Key_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();                 /* 确保 PB 口时钟已开 */

    GPIO_InitStruct.Pin  = KEY_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;           /* 按键接 GND, 必须用上拉,
                                                   * 否则引脚浮空会随机读到 0 */
    HAL_GPIO_Init(KEY_GPIO_GROUP, &GPIO_InitStruct);
}

int Key_Read(void)
{
    if (GPIO_PIN_RESET == HAL_GPIO_ReadPin(KEY_GPIO_GROUP, KEY_GPIO_PIN))
        return 1;
    else
        return 0;
}

void Key_Test(void)
{
    int len;
    int val;

    Key_Init();
    Led_Init();

    while (1)
    {
        LCD_PrintString(0, 0, "Key Test: ");

        val = Key_Read();
        Led_Control(LED_GREEN, val);

        len = LCD_PrintString(0, 2, "Key : ");
        len += LCD_PrintString(len, 2, val ? "Pressed":"Released");
        LCD_ClearLine(len, 2);
    }
}

uint8_t Key_GetNum(void)
{
	uint8_t KeyNum = 0;		//定义变量，默认键码值为0

	if (GPIO_PIN_RESET == HAL_GPIO_ReadPin(KEY_GPIO_GROUP, KEY_GPIO_PIN))			//读PB14输入寄存器的状态，如果为0，则代表按键按下
	{
		HAL_Delay(20);											//延时消抖

		/* 二次确认, 避免毛刺 */
		if (GPIO_PIN_RESET == HAL_GPIO_ReadPin(KEY_GPIO_GROUP, KEY_GPIO_PIN))
		{
			/* 空循环等松手. 注意原来这里多写了一个分号:
			 *   while (...);   <-- 分号让循环体变空, 后面的大括号成了普通代码块,
			 *   只要引脚一直为低就会在这里死等, 整机卡死, 电机代码永远执行不到。*/
			while (GPIO_PIN_RESET == HAL_GPIO_ReadPin(KEY_GPIO_GROUP, KEY_GPIO_PIN))
			{
				HAL_Delay(20);										//等待按键松手
			}
			KeyNum = 1;												//置键码为1
		}
	}
	return KeyNum;
}

