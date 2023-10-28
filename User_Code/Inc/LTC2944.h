#ifndef __LTC_2944
#define __LTC_2944

#define LTC2944_I2C  I2C2

#define LTC2944_Address     0xC8           //库仑计从机地址
//A:状态寄存器
#define ADDR_A      0x00
//B:控制寄存器
#define ADDR_B      0x01
//CD:累积电荷寄存器
#define ADDR_C      0x02
#define ADDR_D      0x03
//EF:充电阀值高
#define ADDR_E      0x04
#define ADDR_F      0x05
//GH:充电阀值低
#define ADDR_G      0x06
#define ADDR_H      0x07

#define Qbat 2648                          //电池总电量

extern float Battery_Percent;

void LTC2944_Init(void);
void LTC2944_I2C_SendByte(uint8_t addr,uint8_t data);
uint8_t LTC2944_I2C_ReadByte(uint8_t addr);
void Get_BatteryVal_Entry(void* parameter);
void Get_BatteryVal_Start(void);
#endif
