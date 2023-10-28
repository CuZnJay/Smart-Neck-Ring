#ifndef __PERIPHERAL_INIT_H
#define __PERIPHERAL_INIT_H

#include "ch32v30x.h"

#define LED_Green_Port GPIOB
#define LED_Green_Pin  GPIO_Pin_4
#define LED_Green_RCC  RCC_APB2Periph_GPIOB

#define LED_Red_Port  GPIOA
#define LED_Red_Pin   GPIO_Pin_15
#define LED_Red_RCC   RCC_APB2Periph_GPIOA

void Private_USARTx_Init(void);
void LED_Init(GPIO_TypeDef* LED_Port , uint16_t LED_Pin , uint32_t LED_RCC_Port);
void Private_ADC_Init(void);
void OLED_I2C_Init(void);
void QMA7981_And_LTC2944_I2C_Init(void);
void Button_GPIO_Init(void);
void Breakage_GPIO_Init(void);

#endif
