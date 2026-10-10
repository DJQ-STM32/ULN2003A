/*
 * driver_key.h
 *
 *  Created on: Aug 20, 2026
 *      Author: 杜家强
 */
#ifndef _DRIVER_KEY_H
#define _DRIVER_KEY_H

#include <stdint.h>

void Key_Init(void);
int Key_Read(void);
void Key_Test(void);
uint8_t Key_GetNum(void);

#endif /* _DRIVER_KEY_H */

