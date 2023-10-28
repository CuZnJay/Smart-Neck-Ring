#include "ch32v30x.h"
#include <rtthread.h>
#include <rthw.h>
#include "drivers/pin.h"
#include <board.h>
#include <string.h>
#include "math.h"
#include "OLED_Font.h"
#include "OLED.h"
#include "Peripheral_Init.h"
#include "OLED_Font.h"

/**
  * @brief  OLED清屏
  * @param  无
  * @retval 无
  */
void OLED_Clear(void)
{
    uint8_t i, j;
    for (j = 0; j < 8; j++)
    {
        OLED_SetCursor(j, 0);
        for(i = 0; i < 128; i++)
        {
            OLED_WriteData(0x00);
        }
    }
}

/**
  * @brief OLED显示一个字符
  * @param (X,Y) 起始行和列位置(Y:0-7)(X:0-127)
  * @param Data 需要显示的数据
  * @param Char_Size 字体大小
  */
void OLED_ShowChar(uint8_t Y, uint8_t X, char Data, uint8_t Char_Size)
{
    uint8_t Height = 0;
    uint8_t Width = 0;
    uint8_t uY_Cnt = 0, uX_Cnt = 0, uData_Cnt = 0;
    switch(Char_Size)
    {
    case 16:
        Height = 2, Width = 8;
        if(Width + X > 128)
            Width = 128 - X;
        if(Height + Y> 8)
            Height = 8 - Y;
        for(uY_Cnt = 0; uY_Cnt < Height; uY_Cnt++)
        {
            OLED_SetCursor(Y + uY_Cnt, X);
            for(uX_Cnt = 0; uX_Cnt < Width; uX_Cnt++)
            {
                OLED_SetCursor(Y + uY_Cnt, X + uX_Cnt);
                OLED_WriteData(OLED_F8x16[Data - ' '][uData_Cnt]);
                uData_Cnt ++;
            }
        }
        break;
    case 48:
        Height = 6, Width = 24;
        if(Width + X > 128)
            Width = 128 - X;
        if(Height + Y> 8)
            Height = 8 - Y;
        for(uY_Cnt = 0; uY_Cnt < Height; uY_Cnt++)
        {
            uData_Cnt = 0;
            OLED_SetCursor(Y + uY_Cnt, X);
            for(uX_Cnt = 0; uX_Cnt < Width; uX_Cnt++)
            {
                OLED_SetCursor(Y + uY_Cnt, X + uX_Cnt);
                OLED_WriteData(OLED_F24X48[(Data - '.') * 6 + uY_Cnt][uData_Cnt]);
                uData_Cnt ++;
            }
        }
        break;
    }
}
/**
  * @brief  OLED显示字符串
  * @param  (X,Y) 起始行和列位置(Y:0-7)(X:0-127)
  * @param  String 要显示的字符串 ;范围：ASCII可见字符
  * @param  Char_Size 字体大小
  * @retval 无
  */
void OLED_ShowString(uint8_t Y, uint8_t X, char *String, uint8_t Char_Size)
{
    uint8_t i;
    switch(Char_Size)
    {
    case 16:
        for (i = 0; String[i] != '\0'; i++)
        {
            OLED_ShowChar(Y, X, String[i], Char_Size);
            X += 8;
            if(X > 128)
                break;
        }
        break;
    case 48:
        for (i = 0; String[i] != '\0'; i++)
        {
            OLED_ShowChar(Y, X, String[i], Char_Size);
            X += 24;
            if(X > 128)
                break;
        }
        break;
    }

}
/**
  * @brief  OLED显示整数
  * @param  (X,Y) 起始行和列位置(Y:0-7)(X:0-127)
  * @param  Num 要显示的数字
  * @param  Char_Size 字体大小
  * @retval 无
  */
void OLED_ShowInt(uint8_t Y, uint8_t X, uint32_t Num, uint8_t Char_Size)
{
    char aIntVal[16] = {'0'};
    sprintf((char*)&aIntVal, "%d", Num);
    OLED_ShowString(Y, X, aIntVal, Char_Size);
}

/**
  * @brief  OLED显示浮点数
  * @param  (X,Y) 起始行和列位置(Y:0-7)(X:0-127)
  * @param  Num 要显示的数字
  * @param  Frac_Bits 保留的小数位数
  * @param  Char_Size 字体大小
  * @retval 无
  */
void OLED_ShowFloat(uint8_t Y, uint8_t X, double Num, uint8_t Frac_Bits, uint8_t Char_Size)
{
    char aFloatVal[16] = {'0'};
    uint8_t uData_Len_Cnt = 0;//数据长度计数
    uint8_t uBuffer_Len_Cnt = 0;//缓存数组赋值计数

    uint32_t FloVal_Int = 0;  //储存浮点型数据整数部分
    double FloVal_Frac = 0.0;  //储存浮点型数据小数部分
    uint32_t FloVal_Frac_32t = 0;//储存浮点型数据小数部分在小数点向右移动后的整数部分

    uint32_t val_seg = 0;   // 数据切分暂存值
    /*判断正负*/
    if(Num < 0)
    {
        Num = -Num;
        aFloatVal[uBuffer_Len_Cnt] = '-';//添加负号
        uBuffer_Len_Cnt++;
    }
    FloVal_Int = (uint32_t)Num;//取整数部分
    FloVal_Frac = Num - FloVal_Int;

    val_seg = FloVal_Int;
    if(FloVal_Int == 0)
    {
        aFloatVal[uBuffer_Len_Cnt] = '0';//打印0
        uBuffer_Len_Cnt++;
    }
    while(val_seg)
    {
        val_seg /= 10;
        uData_Len_Cnt++;
    }
    while(uData_Len_Cnt > 0)
    {
        val_seg = FloVal_Int / (uint32_t)pow(10, uData_Len_Cnt - 1);
        FloVal_Int %= (uint32_t)pow(10, uData_Len_Cnt - 1);
        aFloatVal[uBuffer_Len_Cnt] = (char)('0' + (char)val_seg);
        uBuffer_Len_Cnt++;
        uData_Len_Cnt--;
    }
    aFloatVal[uBuffer_Len_Cnt] = '.';//打印小数点
    uBuffer_Len_Cnt++;

    FloVal_Frac *= pow(10, Frac_Bits);//保留6位小数
    FloVal_Frac_32t = (uint32_t)FloVal_Frac;//取整数部分
    val_seg = FloVal_Frac_32t;
    if(FloVal_Frac_32t == 0)
    {
        aFloatVal[uBuffer_Len_Cnt] = '0';//打印0
        uBuffer_Len_Cnt++;
    }
    while(val_seg)
    {
        val_seg /= 10;
        uData_Len_Cnt++;
    }
    while(uData_Len_Cnt > 0)
    {
        val_seg = FloVal_Frac_32t / (uint32_t)pow(10, uData_Len_Cnt - 1);
        FloVal_Frac_32t %= (uint32_t)pow(10, uData_Len_Cnt - 1);
        aFloatVal[uBuffer_Len_Cnt] = (char)('0' + (char)val_seg);
        uBuffer_Len_Cnt++;
        uData_Len_Cnt--;
    }
    OLED_ShowString(Y, X, aFloatVal, Char_Size);
}

/**
  * @brief  OLED显示浮点数
  * @param  (X,Y) 起始行和列位置(Y:0-7)(X:0-127)
  * @param  Height 图片高度(0 - 7)
  * @param  Width 图片宽度(0 - 127)
  * @param  BMP[] 要显示的图片点阵数组
  * @retval 无
  */
void OLED_DrawBMP(uint8_t Y, uint8_t X, uint8_t Height, uint8_t Width, uint8_t BMP[])
{
    uint8_t uY_Cnt = 0, uX_Cnt = 0;
    uint16_t uData_Cnt = 0;
    if(Width + X > 128)
        Width = 128 - X;
    if(Height + Y> 8)
        Height = 8 - Y;
    for(uY_Cnt = 0; uY_Cnt < Height; uY_Cnt++)
    {
        OLED_SetCursor(Y + uY_Cnt, X);
        for(uX_Cnt = 0; uX_Cnt < Width; uX_Cnt++)
        {
            OLED_WriteData(BMP[uData_Cnt]);
            if(uData_Cnt < (Height * Width))
                uData_Cnt ++;
        }
    }
}

/**
  * @brief  OLED次方函数
  * @retval 返回值等于X的Y次方
  */
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
    uint32_t Result = 1;
    while (Y--)
    {
        Result *= X;
    }
    return Result;
}

/**
  * @brief  I2C发送一个字节
  * @param  addr 发送数据的地址
  * @param  data 要发送的一个字节
  * @retval 无
  */
void OLED_I2C_SendByte(uint8_t addr,uint8_t data)
{
    while(I2C_GetFlagStatus(OLED_I2C, I2C_FLAG_BUSY));

    I2C_GenerateSTART(OLED_I2C, ENABLE);//开启I2C1
    while(!I2C_CheckEvent(OLED_I2C, I2C_EVENT_MASTER_MODE_SELECT));/*EV5,主模式*/

    I2C_Send7bitAddress(OLED_I2C, 0x78, I2C_Direction_Transmitter);//器件地址 -- 默认0x78
    while(!I2C_CheckEvent(OLED_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(OLED_I2C, addr);//寄存器地址
    while (!I2C_CheckEvent(OLED_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(OLED_I2C, data);//发送数据
    while (!I2C_CheckEvent(OLED_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTOP(OLED_I2C, ENABLE);//关闭I2C1总线
}

/**
  * @brief  OLED写命令
  * @param  Command 要写入的命令
  * @retval 无
  */
void OLED_WriteCommand(uint8_t Command)
{
    OLED_I2C_SendByte(0x00, Command);
}

/**
  * @brief  OLED写数据
  * @param  Data 要写入的数据
  * @retval 无
  */
void OLED_WriteData(uint8_t Data)
{
    OLED_I2C_SendByte(0x40, Data);
}

/**
  * @brief  OLED设置光标位置
  * @param  Y 以左上角为原点，向下方向的坐标，范围：0~7
  * @param  X 以左上角为原点，向右方向的坐标，范围：0~127
  * @retval 无
  */
void OLED_SetCursor(uint8_t Y, uint8_t X)
{
	OLED_WriteCommand(0xB0 | Y);					//设置Y位置
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));	//设置X位置低4位
	OLED_WriteCommand(0x00 | (X & 0x0F));			//设置X位置高4位
}

/**
  * @brief  OLED初始化
  * @param  无
  * @retval 无
  */
void OLED_Init(void)
{
	uint32_t i, j;
	
	for (i = 0; i < 1000; i++)			//上电延时
	{
		for (j = 0; j < 1000; j++);
	}
	
	OLED_I2C_Init();			//端口初始化
	
	OLED_WriteCommand(0xAE);	//关闭显示
	
	OLED_WriteCommand(0xD5);	//设置显示时钟分频比/振荡器频率
	OLED_WriteCommand(0x80);
	
	OLED_WriteCommand(0xA8);	//设置多路复用率
	OLED_WriteCommand(0x3F);
	
	OLED_WriteCommand(0xD3);	//设置显示偏移
	OLED_WriteCommand(0x00);
	
	OLED_WriteCommand(0x40);	//设置显示开始行
	
	OLED_WriteCommand(0xA1);	//设置左右方向，0xA1正常 0xA0左右反置
	
	OLED_WriteCommand(0xC8);	//设置上下方向，0xC8正常 0xC0上下反置

	OLED_WriteCommand(0xDA);	//设置COM引脚硬件配置
	OLED_WriteCommand(0x12);
	
	OLED_WriteCommand(0x81);	//设置对比度控制
	OLED_WriteCommand(0xCF);

	OLED_WriteCommand(0xD9);	//设置预充电周期
	OLED_WriteCommand(0xF1);

	OLED_WriteCommand(0xDB);	//设置VCOMH取消选择级别
	OLED_WriteCommand(0x30);

	OLED_WriteCommand(0xA4);	//设置整个显示打开/关闭

	OLED_WriteCommand(0xA6);	//设置正常/倒转显示

	OLED_WriteCommand(0x8D);	//设置充电泵
	OLED_WriteCommand(0x14);

	OLED_WriteCommand(0xAF);	//开启显示
		
	OLED_Clear();				//OLED清屏
}
