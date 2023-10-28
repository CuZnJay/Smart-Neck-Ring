/********************************** (C) COPYRIGHT *******************************
* File Name          : main.c
* Author             : WCH
* Version            : V1.0.0
* Date               : 2021/06/06
* Description        : Main program body.
* Copyright (c) 2021 Nanjing Qinheng Microelectronics Co., Ltd.
* SPDX-License-Identifier: Apache-2.0
*******************************************************************************/
#include "ch32v30x.h"
#include <rtthread.h>
#include <rthw.h>
#include "drivers/pin.h"
#include <board.h>

#include "Peripheral_Init.h"
#include "A9G.h"
#include "my_printf.h"
#include "OLED.h"
#include "thermistor.h"
#include "data_show.h"
#include "Before_Init.h"
#include "QMA7981.h"
#include "LTC2944.h"
#include "led.h"
#include "breakage_alarm.h"

/* Global typedef */

/* Global define */

/* Global Variable */

/*********************************************************************
 * @fn      main
 *
 * @brief   Main program.
 *
 * @return  none
 */
int main(void)
{
    Private_USARTx_Init();
    LED_Init(LED_Red_Port , LED_Red_Pin , LED_Red_RCC);
    LED_Init(LED_Green_Port , LED_Green_Pin , LED_Green_RCC);
    Button_GPIO_Init();
    Breakage_GPIO_Init();
    Private_ADC_Init();
    OLED_I2C_Init();
    QMA7981_And_LTC2944_I2C_Init();

    Before_Init();

    OLED_Init();
    LTC2944_Init();
    QMA7981_Init();

//    rt_pin_write(LED1, 1);
    A9G_MQTT_GPS_Start();
    OLED_And_Button_Start();

    Get_Temperature_Start();
    Get_BatteryVal_Start();
    Get_Movement_Start();
    Breakage_Alarm_Start();

    rt_kprintf("MCU: CH32V307\n");
    rt_kprintf("SysClk: %dHz\n",SystemCoreClock);
    rt_kprintf("www.wch.cn\n");

    return RT_EOK;
}




