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

rt_uint32_t uEventVal_A9G = 0;//事件集的数值

struct rt_event OLED_Data_Refresh_EventSet;//各传感器动态数据的OLED显示刷新事件集
struct rt_event A9G_EventSet;//初始化A9G反馈事件集

rt_mutex_t A9G_Occupy_Mutex = RT_NULL;//指向互斥量的指针

/**
  * @brief 全局事件集、信号量、互斥量的初始化
  */
void Before_Init(void)
{
    rt_err_t OLED_Data_Refresh_EventSet_Ret;//创建各传感器动态数据的OLED显示刷新事件集return值
    rt_err_t A9G_EventSet_Ret;//创建A9G反馈事件集return值

    /* 创建一个动态互斥量 */
    A9G_Occupy_Mutex = rt_mutex_create("A9G_Occupy_Mutex_Name", RT_IPC_FLAG_PRIO);
    if (A9G_Occupy_Mutex == RT_NULL)
    {
        rt_kprintf("create A9G_Occupy_Mutex failed.\r\n");
    }

    /* 初始化A9G反馈事件集对象 */
    A9G_EventSet_Ret = rt_event_init(&A9G_EventSet, "A9G_EventSet_Name", RT_IPC_FLAG_PRIO);
    if (A9G_EventSet_Ret != RT_EOK)
    {
        rt_kprintf("init A9G_EventSet failed.\n");
    }

    /* 初始化各传感器数据的OLED显示刷新事件集对象 */
    OLED_Data_Refresh_EventSet_Ret = rt_event_init(&OLED_Data_Refresh_EventSet, "OLED_Data_Refresh_EventSet_Name", RT_IPC_FLAG_PRIO);
    if (OLED_Data_Refresh_EventSet_Ret != RT_EOK)
    {
        rt_kprintf("init OLED_Data_Refresh_EventSet failed.\n");
    }
}

