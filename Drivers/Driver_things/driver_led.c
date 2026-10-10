/*
 * driver_led.c
 *
 *  Created on: Aug 20, 2026
 *      Author: 杜家强
 */

#include "driver_led.h"

#include "main.h"

int Led_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /*Configure GPIO pin Output Level */
    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);

    /*Configure GPIO pin : PC13 */
    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    return 0;
}

int Led_Control(int which, int on)
{
    if (on)
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
    else
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    return 0;
}

void Led_Test(void)
{
    Led_Init();

    while (1)
    {
        Led_Control(LED_GREEN, 1);
        HAL_Delay(500);

        Led_Control(LED_GREEN, 0);
        HAL_Delay(500);
   }
}


