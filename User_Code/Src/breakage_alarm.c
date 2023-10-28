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
#include "common_setting.h"

uint8_t Ctrl_Z = 0x1A;//A9G停止操作字符

static struct rt_semaphore Breakage_Sem;//颈环损坏信号量
static struct rt_semaphore Breakage_Scan_Reset_Sem;//损坏检测重置信号量

/**
  * @brief 颈环损坏回调函数
  */
void Damage_Callback(void *args)
{
    rt_sem_release(&Breakage_Sem);
}

/**
  * @brief 损坏检测重置回调函数
  */
void Breakage_Scan_Reset_Callback(void *args)
{
    rt_sem_release(&Breakage_Scan_Reset_Sem);
}

/**
  * @brief 破拆警告线程入口函数
  */
void Breakage_Alarm_Entry(void *paramater)
{
    uint8_t uCycle_Cnt = 0;

    /* 设置破拆检测引脚为输入模式 */
    rt_pin_mode(Breakage_Scan_PIN, PIN_MODE_INPUT_PULLDOWN);
    rt_pin_mode(Breakage_Scan_Reset_Button_Pin, PIN_MODE_INPUT_PULLUP);

    /* 绑定破拆检测引脚中断，设置为下降沿模式，回调函数名为beep_on */
    rt_pin_attach_irq(Breakage_Scan_PIN, PIN_IRQ_MODE_RISING, Damage_Callback, RT_NULL);
    rt_pin_attach_irq(Breakage_Scan_Reset_Button_Pin, PIN_IRQ_MODE_FALLING, Breakage_Scan_Reset_Callback, RT_NULL);

    /* 使能中断 */
    rt_pin_irq_enable(Breakage_Scan_PIN, PIN_IRQ_ENABLE);

    while(1)
    {
        /* 破拆消息上报阿里云 */
        rt_mutex_take(A9G_Occupy_Mutex, RT_WAITING_FOREVER);
        my_printf("AT+MQTTPUB=\"/sys/in32QSarJUf/Device_01/thing/event/property/post\",\"{\\22id\\22:\\22123\\22,\\22version\\22:\\221.0\\22,\\22params\\22:{\\22Device_State\\22:\\22%s\\22},\\22method\\22:\\22thing.event.property.post\\22}\",0,0,0\r\n",
                "Normal");//上传数据到阿里云
        rt_thread_mdelay(1000);
        rt_device_write(A9G_UART_Serial, 0, "AT+GPS=?\r\n", sizeof("AT+GPS=?\r\n"));
        rt_event_recv(&A9G_EventSet,
                      OK_FeedBack_Event,
                     (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                      RT_WAITING_FOREVER,
                      &uEventVal_A9G);
        rt_mutex_release(A9G_Occupy_Mutex);

        /* 阻塞方式等待损坏检测重置按键点击信号量 */
        rt_sem_take(&Breakage_Sem, RT_WAITING_FOREVER);
        while(rt_sem_take(&Breakage_Sem, RT_WAITING_FOREVER) == RT_EOK)
        {
            uCycle_Cnt = 8;//破拆信号量消耗的次数

            /* 循环消耗破拆信号量来判断是不是接触不良或者误触 */
            while(uCycle_Cnt > 0)
            {
                rt_sem_take(&Breakage_Sem, RT_WAITING_NO);
                uCycle_Cnt --;
            }

            /* 消耗次数到达后还有剩余破拆信号量则颈环出现破拆情况 */
            if(rt_sem_take(&Breakage_Sem, RT_WAITING_NO) == RT_EOK)
            {
                /* 失能破拆检测引脚 */
                rt_pin_irq_enable(Breakage_Scan_PIN, PIN_IRQ_DISABLE);

                /* 循环消耗破拆信号量 */
                while(1)
                {
                    if(rt_sem_take(&Breakage_Sem, RT_WAITING_NO) != RT_EOK)
                        break;
                }

                /* 使能损坏重置按键引脚 */
                rt_pin_irq_enable(Breakage_Scan_Reset_Button_Pin, PIN_IRQ_ENABLE);
                break;
            }
            else
                continue;
        }

        /* 通过电话和短信提示颈环被破拆并且上报消息到阿里云 */
        rt_mutex_take(A9G_Occupy_Mutex, RT_WAITING_FOREVER);
        rt_device_write(A9G_UART_Serial, 0, "ATD15860597877\r\n", sizeof("ATD15860597877\r\n"));
        rt_thread_mdelay(500);
        rt_device_write(A9G_UART_Serial, 0, "AT+CMGF=1\r\n", sizeof("AT+CMGF=1\r\n"));
        rt_thread_mdelay(500);
        rt_device_write(A9G_UART_Serial, 0, "AT+CMGS=\"15860597877\"\r\n", sizeof("AT+CMGS=\"15860597877\"\r\n"));
        rt_thread_mdelay(500);
        rt_device_write(A9G_UART_Serial, 0, "Neck Ring Is Broken\r\n", sizeof("Neck Ring Is Broken\r\n"));
        rt_thread_mdelay(500);
        rt_device_write(A9G_UART_Serial, 0, &Ctrl_Z, 1);
        rt_thread_mdelay(500);
        my_printf("AT+MQTTPUB=\"/sys/in32QSarJUf/Device_01/thing/event/property/post\",\"{\\22id\\22:\\22123\\22,\\22version\\22:\\221.0\\22,\\22params\\22:{\\22Device_State\\22:\\22%s\\22},\\22method\\22:\\22thing.event.property.post\\22}\",0,0,0\r\n",
                "Damaged");//上传数据到阿里云
        rt_thread_mdelay(1000);
        rt_device_write(A9G_UART_Serial, 0, "AT+GPS=?\r\n", sizeof("AT+GPS=?\r\n"));
        rt_event_recv(&A9G_EventSet,
                      OK_FeedBack_Event,
                     (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                      RT_WAITING_FOREVER,
                      &uEventVal_A9G);
        rt_mutex_release(A9G_Occupy_Mutex);

        /* 阻塞方式等待损坏重置按键信号量 */
        rt_sem_take(&Breakage_Scan_Reset_Sem, RT_WAITING_FOREVER);
        /* 按键消抖 */
        rt_thread_mdelay(150);
        while(1)
        {
            if(rt_sem_take(&Breakage_Scan_Reset_Sem, RT_WAITING_NO) != RT_EOK)
                break;
        }

        /* 刷新后重新开启破拆检测引脚中断和关闭损坏检测重置按键中断 */
        rt_pin_irq_enable(Breakage_Scan_Reset_Button_Pin, PIN_IRQ_DISABLE);
        rt_pin_irq_enable(Breakage_Scan_PIN, PIN_IRQ_ENABLE);
    }

}

/**
  * @brief 破拆警告启动
  */
void Breakage_Alarm_Start(void)
{
    static rt_thread_t Breakage_Alarm_Thread;//定义线程

    /* 初始化信号量 */
    rt_sem_init(&Breakage_Sem, "Breakage_Sem_Name", 0, RT_IPC_FLAG_FIFO);
    rt_sem_init(&Breakage_Scan_Reset_Sem, "Breakage_Scan_Reset_Sem", 0, RT_IPC_FLAG_FIFO);

    /* 动态创建并开启线程 */
    Breakage_Alarm_Thread = rt_thread_create("Breakage_Alarm_Thread_Name",
                                             Breakage_Alarm_Entry,
                                             RT_NULL,
                                             STACK_SIZE_1024,
                                             PRIORITY_20,
                                             TIMESLICE_5);
    if(Breakage_Alarm_Thread != RT_NULL)
        rt_thread_startup(Breakage_Alarm_Thread);
}
