#include <rtthread.h>

#include <rtdbg.h>
#include <board.h>
#include <drivers/pin.h>
#include <stdio.h>
#include <math.h>
#include "ch32v30x.h"
#include <rtthread.h>
#include <rthw.h>

#include "A9G.h"
#include "my_printf.h"
#include "OLED.h"
#include "thermistor.h"
#include "common_setting.h"
#include "led.h"
#include "Before_Init.h"

rt_adc_device_t Thermistor_ADC_Dev;//定义热敏电阻ADC设备

double Temp_Value = 0.0;//温度值

/**
  * @brief 获取温度线程入口函数
  */
void Get_Temperature_Entry(void* parameter)
{
    rt_uint16_t uTemp_ADC_Value;//电压分压在热敏电阻的AD值
    double Before_Temp_Val = 0.0;//之前的温度值

    rt_uint8_t uTemp_Val_Int = 0;//温度值的整数部分
    rt_uint8_t uTemp_Val_Frac = 0;//温度值的小数部分
    double Temp_Val_Roll = 0.0;//温度值在转换成整数时的缓存值

    double Ohm_Value = 0.0;//热敏电阻值
    double a0 = 129.4905, a1 = -56.86939, a2 = 18.12105, a3 = -3.50579, a4 = 0.41168, a5 = -0.03, a6 = 0.00136, a7 = -3.73E-05, a8 = 5.66E-07, a9 = -3.64E-09;//多项式参数

    rt_err_t Thermistor_ADC_Enable_Ret = RT_EOK;//开启热敏电阻ADC时的返回值

    while(1)
    {
        /* 使能热敏电阻ADC设备 */
        Thermistor_ADC_Enable_Ret = rt_adc_enable(Thermistor_ADC_Dev, Thermistor_ADC_Channel);
        if(Thermistor_ADC_Enable_Ret != RT_EOK)
            rt_kprintf("Thermistor_ADC enable fail");
        rt_thread_mdelay(3000);

        /* 读取采样值 */
        uTemp_ADC_Value = rt_adc_read(Thermistor_ADC_Dev, Thermistor_ADC_Channel);

        /* 转换为对应电压值 */
        Ohm_Value = 10 * uTemp_ADC_Value / (4096 - uTemp_ADC_Value);

        Temp_Value = a9*pow(Ohm_Value,9) + a8*pow(Ohm_Value,8) + a7*pow(Ohm_Value,7) + a6*pow(Ohm_Value,6) + a5*pow(Ohm_Value,5) + a4*pow(Ohm_Value,4) + a3*pow(Ohm_Value,3) + a2*pow(Ohm_Value,2) + a1*pow(Ohm_Value,1) + a0;

        uTemp_Val_Int = (rt_uint8_t)Temp_Value;
        Temp_Val_Roll = Temp_Value - uTemp_Val_Int;
        uTemp_Val_Frac = (rt_uint8_t)(Temp_Val_Roll * 10);

        rt_event_send(&OLED_Data_Refresh_EventSet, Temperature_Refresh_Event);
        if(Before_Temp_Val != Temp_Value)
        {
            /* 温度上报阿里云 */
            rt_mutex_take(A9G_Occupy_Mutex, RT_WAITING_FOREVER);
            my_printf("AT+MQTTPUB=\"/sys/in32QSarJUf/Device_01/thing/event/property/post\",\"{\\22id\\22:\\22123\\22,\\22version\\22:\\221.0\\22,\\22params\\22:{\\22Temperature\\22:%d.%d},\\22method\\22:\\22thing.event.property.post\\22}\",0,0,0\r\n",
                    uTemp_Val_Int,
                    uTemp_Val_Frac);//上传数据到阿里云

            rt_thread_mdelay(1000);
            rt_device_write(A9G_UART_Serial, 0, "AT+GPS=?\r\n", sizeof("AT+GPS=?\r\n"));
            rt_event_recv(&A9G_EventSet,
                          OK_FeedBack_Event,
                         (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                          RT_WAITING_FOREVER,
                          &uEventVal_A9G);
            rt_mutex_release(A9G_Occupy_Mutex);
        }
        Before_Temp_Val = Temp_Value;
    }
}

/**
  * @brief 获取温度开启函数
  */
void Get_Temperature_Start(void)
{
    static struct rt_thread Get_Temperature_Thread;//定义线程
    ALIGN(RT_ALIGN_SIZE)
    static char Get_Temperature_Thread_Stack[1024];//创建线程栈，并且地址4字节对齐（动态创建线程无需这一步骤）

    /* 查找设备 */
    Thermistor_ADC_Dev = (rt_adc_device_t)rt_device_find(Thermistor_ADC);
    if (Thermistor_ADC_Dev == RT_NULL)
    {
        rt_kprintf("find %s failed!\n", Thermistor_ADC);
    }

    /* 静态创建并开启线程 */
    rt_thread_init(&Get_Temperature_Thread,
                   "Get_Temperature_Thread_Name",
                    Get_Temperature_Entry,
                    RT_NULL,
                    &Get_Temperature_Thread_Stack[0],
                    STACK_SIZE_1024,
                    PRIORITY_17, TIMESLICE_5);
    rt_thread_startup(&Get_Temperature_Thread);
}
