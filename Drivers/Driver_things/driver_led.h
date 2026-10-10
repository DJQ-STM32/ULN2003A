/*
 * driver_led.h
 *
 *  Created on: Aug 20, 2026
 *      Author: 杜家强
 */

#ifndef _DRIVER_LED_H
#define _DRIVER_LED_H

#define LED_GREEN 1  /* 哪个LED */

int Led_Init(void);

int Led_Control(int which, int on);

void Led_Test(void);

#endif /* _DRIVER_LED_H */
