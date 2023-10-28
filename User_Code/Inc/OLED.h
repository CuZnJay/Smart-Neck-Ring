#ifndef __OLED_H
#define __OLED_H

#define OLED_I2C I2C1

void OLED_Init(void);
void OLED_Clear(void);
void OLED_ShowChar(uint8_t Y, uint8_t X, char Data, uint8_t Char_Size);
void OLED_ShowString(uint8_t Y, uint8_t X, char *String, uint8_t Char_Size);
void OLED_ShowInt(uint8_t Y, uint8_t X, uint32_t Num, uint8_t Char_Size);
void OLED_ShowFloat(uint8_t Y, uint8_t X, double Num, uint8_t Frac_Bits, uint8_t Char_Size);
void OLED_SetCursor(uint8_t Y, uint8_t X);
void OLED_WriteData(uint8_t Data);
void OLED_WriteCommand(uint8_t Command);
void OLED_I2C_SendByte(uint8_t addr,uint8_t data);
void OLED_DrawBMP(uint8_t Y, uint8_t X, uint8_t Height, uint8_t Width, uint8_t BMP[]);



#endif
