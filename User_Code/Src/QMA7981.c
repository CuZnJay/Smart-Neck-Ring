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
#include "led.h"
#include "Common_Setting.h"

uint32_t uStep_Sum = 0;//步数记录

/**
  * @brief 计步线程入口函数
  */
void QMA7981_Step_Count_Entry(void* paramater)
{
    uint32_t uStep_Reg1 = 0, uStep_Reg2 = 0, uStep_Reg3 = 0;//QMA7981中记录步数的寄存器
    uint32_t uBefore_Step_Sum = 0;//之前的步数记录

    while(1)
    {
        uStep_Reg1 = (uint32_t)QMA7981_I2C_ReadByte(ADDR_STEP_CNT1);    //读步数寄存器低位1
        uStep_Reg2 = (uint32_t)QMA7981_I2C_ReadByte(ADDR_STEP_CNT2);    //读步数寄存器低位2
        uStep_Reg3 = (uint32_t)QMA7981_I2C_ReadByte(ADDR_STEP_CNT3);     //读步数寄存器高位1

        uStep_Sum = uStep_Reg1 | (uStep_Reg2 << 8) | (uStep_Reg3 << 16);

        rt_event_send(&OLED_Data_Refresh_EventSet, Movement_Refresh_Event);

        /* 数据上报阿里云 */
        rt_mutex_take(A9G_Occupy_Mutex, RT_WAITING_FOREVER);
        my_printf("AT+MQTTPUB=\"/sys/in32QSarJUf/Device_01/thing/event/property/post\",\"{\\22id\\22:\\22123\\22,\\22version\\22:\\221.0\\22,\\22params\\22:{\\22Movement\\22:%d},\\22method\\22:\\22thing.event.property.post\\22}\",0,0,0\r\n",
                uStep_Sum);
        rt_thread_mdelay(1000);
        rt_device_write(A9G_UART_Serial, 0, "AT+GPS=?\r\n", sizeof("AT+GPS=?\r\n"));
        rt_event_recv(&A9G_EventSet,
                      OK_FeedBack_Event,
                     (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                      RT_WAITING_FOREVER,
                      &uEventVal_A9G);
        rt_mutex_release(A9G_Occupy_Mutex);

        if(uBefore_Step_Sum != uStep_Sum)
        {
            rt_mutex_take(A9G_Occupy_Mutex, RT_WAITING_FOREVER);
            my_printf("AT+MQTTPUB=\"/sys/in32QSarJUf/Device_01/thing/event/property/post\",\"{\\22id\\22:\\22123\\22,\\22version\\22:\\221.0\\22,\\22params\\22:{\\22Movement\\22:%d},\\22method\\22:\\22thing.event.property.post\\22}\",0,0,0\r\n",
                    uStep_Sum);//上传数据到阿里云

            rt_thread_mdelay(1000);
            rt_device_write(A9G_UART_Serial, 0, "AT+GPS=?\r\n", sizeof("AT+GPS=?\r\n"));
            rt_event_recv(&A9G_EventSet,
                          OK_FeedBack_Event,
                         (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                          RT_WAITING_FOREVER,
                          &uEventVal_A9G);
            rt_mutex_release(A9G_Occupy_Mutex);
        }
        uBefore_Step_Sum = uStep_Sum;
        rt_thread_mdelay(1000);
    }
}

/**
  * @brief 计步开启
  */
void Get_Movement_Start(void)
{
    static struct rt_thread Get_Movement_Thread;//定义线程
    ALIGN(RT_ALIGN_SIZE)
    static char Get_Movement_Thread_Stack[1024];//创建线程栈，并且地址4字节对齐（动态创建线程无需这一步骤）


    /* 静态创建并开启线程 */
    rt_thread_init(&Get_Movement_Thread,
                   "Get_Movement_Thread_Name",
                   QMA7981_Step_Count_Entry,
                    RT_NULL,
                    &Get_Movement_Thread_Stack[0],
                    STACK_SIZE_1024,
                    PRIORITY_18, TIMESLICE_5);
    rt_thread_startup(&Get_Movement_Thread);
}

/**
  * @brief QMA7981初始化
  */
void QMA7981_Init(void)
{
    QMA7981_I2C_SendByte(ADDR_PM, 0xC0);//进入活动模式
    QMA7981_I2C_SendByte(ADDR_STEP_CONF0, 0x94);//使能计步寄存器，样本设置为96
    QMA7981_I2C_SendByte(ADDR_STEP_CONF1, 0x01);//减少误步
    QMA7981_I2C_SendByte(ADDR_STEP_CONF2, 0x03);//时间窗口低
    QMA7981_I2C_SendByte(ADDR_STEP_CONF3, 0xAA);//时间窗口高
    QMA7981_I2C_SendByte(ADDR_INT_EN0, 0xF9);//使能SIG_STEP和计步器中断
    QMA7981_I2C_SendByte(ADDR_SIG_STEP_TH, 0x05);//设置计步器计数中断阈值
    QMA7981_I2C_SendByte(ADDR_TH, 0x09);//设置步频，峰值，峰峰值
}

/**
  * @brief QMA7981清除步数寄存器
  */
void QMA7981_clear(void)
{
    QMA7981_I2C_SendByte(ADDR_STEP_CONF1, 0x82); //清零步数寄存器
}

/**
  * @brief QMA7981发送1个字节数据
  * @param addr 要发送的寄存器地址
  * @param data 要发送的数据
  */
void QMA7981_I2C_SendByte(uint8_t addr,uint8_t data)
{
    while(I2C_GetFlagStatus(QMA7981_I2C, I2C_FLAG_BUSY));

    I2C_GenerateSTART(QMA7981_I2C, ENABLE);//开启I2C2
    while(!I2C_CheckEvent(QMA7981_I2C, I2C_EVENT_MASTER_MODE_SELECT));/*EV5,主模式*/

    I2C_Send7bitAddress(QMA7981_I2C, 0x24, I2C_Direction_Transmitter);//器件地址 0x24
    while(!I2C_CheckEvent(QMA7981_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(QMA7981_I2C, addr);//寄存器地址
    while (!I2C_CheckEvent(QMA7981_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(QMA7981_I2C, data);//发送数据
    while (!I2C_CheckEvent(QMA7981_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTOP(QMA7981_I2C, ENABLE);//关闭I2C1总线
}

/**
  * @brief QMA7981发送1个字节数据
  * @param addr 要读取的寄存器地址
  * @retval uData 读取到的数据
  */
uint8_t QMA7981_I2C_ReadByte(uint8_t addr)
{
    while(I2C_GetFlagStatus(QMA7981_I2C, I2C_FLAG_BUSY));

    I2C_GenerateSTART(QMA7981_I2C, ENABLE);//开启I2C1
    while(!I2C_CheckEvent(QMA7981_I2C, I2C_EVENT_MASTER_MODE_SELECT));/*EV5,主模式*/

    I2C_Send7bitAddress(QMA7981_I2C, 0x24, I2C_Direction_Transmitter);//器件地址 0x24
    while(!I2C_CheckEvent(QMA7981_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(QMA7981_I2C, addr);//寄存器地址
    while (!I2C_CheckEvent(QMA7981_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTART(QMA7981_I2C, ENABLE);//开启I2C1
    while(!I2C_CheckEvent(QMA7981_I2C, I2C_EVENT_MASTER_MODE_SELECT));/*EV5,主模式*/

    I2C_Send7bitAddress(QMA7981_I2C, 0x24, I2C_Direction_Receiver);//器件地址 0x24
    while(!I2C_CheckEvent(QMA7981_I2C, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

    I2C_AcknowledgeConfig(QMA7981_I2C, DISABLE);
    I2C_GenerateSTOP(QMA7981_I2C,ENABLE);

    while (!I2C_CheckEvent(QMA7981_I2C, I2C_EVENT_MASTER_BYTE_RECEIVED));
    uint8_t uData = I2C_ReceiveData(QMA7981_I2C);

    I2C_AcknowledgeConfig(QMA7981_I2C, ENABLE);

    return uData;
}
