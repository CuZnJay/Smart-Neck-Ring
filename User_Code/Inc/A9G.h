#ifndef __A9G_H
#define __A9G_H

#include "ch32v30x.h"
#include <rtthread.h>
#include <rtdbg.h>
#include <board.h>
#include <rtdevice.h>

#define A9G_UART                "uart2"
#define Interaction_UART        "uart1"

#define OK_FeedBack_Event       (1 << 0)
#define A9G_Init_Error_Event    (1 << 1)
#define Location_Error_Event    (1 << 2)

#define RxBuffer_MAX            512

#define Location_Start          1
#define Location_Close          0



/* 串口设备句柄 */
extern rt_device_t A9G_UART_Serial;
extern rt_device_t Interaction_UART_Serial;

/* 转化后的浮点型经纬度数据 */
extern double fLati_Val;//纬度
extern double fLongi_Val;//经度

/* 阿里云连接参数结构体 */
typedef struct
{
    char* ProductKey;
    char* DeviceName;
    char* DeviceSecret;
    char* clientId;
    char* password;
}Device_Property;

int A9G_MQTT_GPS_Start(void);
void Location_To_Float(char* p_array);
void Location_Apart_Int_Frac_Send(void);
#endif
