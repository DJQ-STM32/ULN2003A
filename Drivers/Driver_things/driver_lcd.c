/*
 * driver_lcd.c
 *
 *  Created on: Aug 20, 2026
 *      Author: 杜家强
 */

#include "driver_oled.h"

/*
 *  函数名：LCD_Init
 *  功能描述：初始化LCD
 *  输入参数：无
 *  输出参数：无
 *  返回值：无
 */
void LCD_Init(void)
{
    OLED_Init();
}


/*
 *  函数名：LCD_Clear
 *  功能描述：清屏函数
 *  输入参数：无
 *  输出参数：无
 *  返回值：无
*/
void LCD_Clear(void)
{
    OLED_Clear();
}

/*
 *  函数名：LCD_PutChar
 *  功能描述：显示一个字符
 *  输入参数：x --> x坐标
 *            y --> y坐标
 *            c -->   显示的字符
 *  输出参数：无
 *  返回值：无
*/
void LCD_PutChar(uint8_t x, uint8_t y, char c)
{
    OLED_PutChar(x, y, c);
}


/*
 *  函数名：LCD_PrintString
 *  功能描述：显示一个字符串
 *  输入参数：x --> x坐标
 *            y --> y坐标
 *            str -->   显示的字符串
 *  输出参数：无
 *  返回值：打印了多少个字符
 */
int LCD_PrintString(uint8_t x, uint8_t y, const char *str)
{
   return OLED_PrintString(x, y, str);
}

void LCD_PrintStringClean(uint8_t x, uint8_t y, const char *str, uint8_t *last_len)
{
	uint8_t len = (uint8_t)LCD_PrintString(x, y, str);

	while (len < *last_len)
	{
		LCD_PutChar(x + len, y, ' ');
		len++;
	}
	*last_len = len;
}

/*
 *  函数名：OLED_ClearLine
 *  功能描述：清除一行
 *  输入参数：x - 从这里开始
 *            y - 清除这行
 *  输出参数：无
 *  返回值：无
 */
void LCD_ClearLine(uint8_t x, uint8_t y)
{
   OLED_ClearLine(x, y);
}

/*
 *  LCD_PrintHex
 *  功能描述：以16进制显示数值
 *  输入参数：x - x坐标
 *            y - y坐标
 *            val -   显示的数据
 *            pre -   非零时显示"0x"前缀
 *  输出参数：无
 *  返回值：打印了多少个字符
 */
int LCD_PrintHex(uint8_t x, uint8_t y, uint32_t val, uint8_t pre)
{
    return OLED_PrintHex(x, y, val, pre);
}

/*
 *  LCD_PrintSignedVal
 *  功能描述：以10进制显示一个数值
 *  输入参数：x --> x坐标(0~15)
 *            y --> y坐标(0~7)
 *            val -->   显示的数据
 *  输出参数：无
 *  返回值：打印了多少个字符
 */
int LCD_PrintSignedVal(uint8_t x, uint8_t y, int32_t val)
{
    return OLED_PrintSignedVal(x, y, val);
}

/*
 *  函数名：LCD_PrintValClean
 *  功能描述：在(x,y)显示十进制数值，并擦掉"上次比这次多出来的高位字符"
 *  输入参数：x  - x坐标
 *            y  - y坐标
 *            val - 要显示的数值
 *            last_len - 指向"上次打印的字符数"，函数内会更新它
 *  输出参数：无
 *  返回值：无
 *  说明：LCD_PrintSignedVal 只覆盖写本次的位数，不会清旧字符。
 *        数值从 3 位变 2 位时(如 150 -> 60)，不擦就会残留成 "600"。
 *        所以显示方必须自己记住上次长度并补空格。
 */
void LCD_PrintValClean(uint8_t x, uint8_t y, int32_t val, uint8_t *last_len)
{
	uint8_t len = (uint8_t)LCD_PrintSignedVal(x, y, val);

	while (len < *last_len)
	{
		LCD_PutChar(x + len, y, ' ');
		len++;
	}
	*last_len = len;
}

/**********************************************************************
 * 函数名称： LCD_Test
 * 功能描述： LCD测试程序
 * 输入参数： 无
 * 输出参数： 无
 *            无
 * 返 回 值： 0 - 成功, 其他值 - 失败
 * 修改日期        版本号     修改人        修改内容
 * -----------------------------------------------
 * 2023/08/04        V1.0     韦东山       创建
 ***********************************************************************/
void LCD_Test(void)
{
    OLED_Test();
}




