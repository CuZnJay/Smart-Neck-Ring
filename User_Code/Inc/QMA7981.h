#ifndef __QMA7981_H
#define __QMA7981_H

#define QMA7981_I2C         I2C2

#define QMA7981_Address     0x24           //计步模块从机地址

//步数寄存器
#define ADDR_STEP_CNT1      0x07      //低位1
#define ADDR_STEP_CNT2      0x08      //低位2
#define ADDR_STEP_CNT3      0x0E      //高位
//模式寄存器
#define ADDR_PM             0x11
//
#define ADDR_STEP_CONF0     0x12      //使能步数寄存器
#define ADDR_STEP_CONF1     0x13      //清零步数寄存器，设置两个连续采样的加速度变化阈值
//时间窗口寄存器
#define ADDR_STEP_CONF2     0x14      //时间窗口低
#define ADDR_STEP_CONF3     0x15      //时间窗口高
//使能中断寄存器
#define ADDR_INT_EN0        0x16
//显著步长寄存器
#define ADDR_SIG_STEP_TH    0x1D
//计步设置寄存器
#define ADDR_TH             0x1F      //设置计步模式，峰值，峰峰值

extern uint32_t uStep_Sum;

void QMA7981_Init(void);
void QMA7981_I2C_SendByte(uint8_t addr,uint8_t data);
uint8_t QMA7981_I2C_ReadByte(uint8_t addr);
void Get_Movement_Start(void);
void QMA7981_clear(void);
#endif
