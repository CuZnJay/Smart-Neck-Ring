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
#include "LTC2944.h"
#include "Common_Setting.h"

float Battery_Percent = 100;//电量值

/**
  * @brief 库仑计LTC2944初始化
  */
void LTC2944_Init(void)
{
    LTC2944_I2C_SendByte(ADDR_B, 0x3D);    //关闭模拟
    LTC2944_I2C_SendByte(ADDR_C, 0xFF);
    /*
     *  配置控制寄存器
     *  ADC模式[7：6]：关闭;  (00)
     *  预分频[5:3]:4096；(111)
     *  ALCC模式[2:1]:报警模式；(10)
     *  模拟状态[0]:打开;  (1)
     * */
    LTC2944_I2C_SendByte(ADDR_B, 0x3C);    //0011 1100 控制寄存器

    /*
     *  对寄存器CD EF GF 进行写操作；
     *  CD默认:FFFF; 低电量提醒：剩余10％提醒
     *  GH:电池低电量提醒
     * */
    LTC2944_I2C_SendByte(ADDR_G, 0x9C);
    LTC2944_I2C_SendByte(ADDR_H, 0xBB);
}

/**
  * @brief 获取电量 入口函数
  */
void Get_BatteryVal_Entry(void* parameter)
{
    float Before_Battery_Percent = 0.0;

    uint8_t uBattery_Percent_Int = 0;
    uint8_t uBattery_Percent_Frac = 0;
    float Battery_Percent_Roll = 0.0;

//    uint16_t uLost_Battery_Val = 0;

    while(1)
    {
//        uint16_t C_Battery_Val = 0;
//        uint16_t D_Battery_Val = 0;

//        C_Battery_Val = LTC2944_I2C_ReadByte(ADDR_C);
//        D_Battery_Val = LTC2944_I2C_ReadByte(ADDR_D);
//        uLost_Battery_Val = (C_Battery_Val << 8) | D_Battery_Val;

//        Battery_Percent = (Qbat - (0xFFFF - uLost_Battery_Val) * 0.34) * 100 / Qbat;

        uBattery_Percent_Int = (uint8_t)Battery_Percent;
        Battery_Percent_Roll = Battery_Percent - uBattery_Percent_Int;
        uBattery_Percent_Frac = (uint8_t)(Battery_Percent_Roll * 10);

        if(Before_Battery_Percent != Battery_Percent)
        {
            /* 电量刷新事件，刷新OLED上的电量显示 */
            rt_event_send(&OLED_Data_Refresh_EventSet, Battery_Refresh_Event);

            /* 占用互斥量，防止A9G数据发送阿里云的通道被占用 */
            rt_mutex_take(A9G_Occupy_Mutex, RT_WAITING_FOREVER);

            /* 电量数据上传阿里云 */
            my_printf("AT+MQTTPUB=\"/sys/in32QSarJUf/Device_01/thing/event/property/post\",\"{\\22id\\22:\\22123\\22,\\22version\\22:\\221.0\\22,\\22params\\22:{\\22BatteryPercentage\\22:%d.%d},\\22method\\22:\\22thing.event.property.post\\22}\",0,0,0\r\n",
                    uBattery_Percent_Int,
                    uBattery_Percent_Frac);
            rt_thread_mdelay(1000);

            /* 发送定位状态询问消息，目的只是为了解决发送数据后反馈消息接收不完全的问题 */
            rt_device_write(A9G_UART_Serial, 0, "AT+GPS=?\r\n", sizeof("AT+GPS=?\r\n"));

            /* 接收多余的OK事件反馈 */
            rt_event_recv(&A9G_EventSet,
                          OK_FeedBack_Event,
                         (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                          RT_WAITING_FOREVER,
                          &uEventVal_A9G);

            /* 释放互斥量，此时别功能可占用A9G模块  */
            rt_mutex_release(A9G_Occupy_Mutex);
        }
        rt_thread_mdelay(60000);
    }
}

/**
  * @brief 获取电量开始函数
  */
void Get_BatteryVal_Start(void)
{
    static struct rt_thread Get_BatteryVal_Thread;//定义线程

    ALIGN(RT_ALIGN_SIZE)
    static char Get_BatteryVal_Thread_Stack[1024];//创建线程栈，并且地址4字节对齐（动态创建线程无需这一步骤）

    /* 静态创建并开启电量获取线程 */
    rt_thread_init(&Get_BatteryVal_Thread,
                   "Get_BatteryVal_Thread_Name",
                   Get_BatteryVal_Entry,
                    RT_NULL,
                    &Get_BatteryVal_Thread_Stack[0],
                    STACK_SIZE_1024,
                    PRIORITY_19, TIMESLICE_5);
    rt_thread_startup(&Get_BatteryVal_Thread);
}

/**
  * @brief 发送一字节数据给LTC2944
  * @param addr 寄存器地址
  * @param data 发送的数据
  */
void LTC2944_I2C_SendByte(uint8_t addr,uint8_t data)
{
    while(I2C_GetFlagStatus(LTC2944_I2C, I2C_FLAG_BUSY));

    I2C_GenerateSTART(LTC2944_I2C, ENABLE);//开启I2C1
    while(!I2C_CheckEvent(LTC2944_I2C, I2C_EVENT_MASTER_MODE_SELECT));/*EV5,主模式*/

    I2C_Send7bitAddress(LTC2944_I2C, 0xC8, I2C_Direction_Transmitter);//器件地址0xC8
    while(!I2C_CheckEvent(LTC2944_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(LTC2944_I2C, addr);//寄存器地址
    while (!I2C_CheckEvent(LTC2944_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_SendData(LTC2944_I2C, data);//发送数据
    while (!I2C_CheckEvent(LTC2944_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTOP(LTC2944_I2C, ENABLE);//关闭I2C1总线
}

/**
  * @brief 读取LTC2944的数据
  * @param addr 寄存器地址
  * @retval uData 寄存器中的数据
  */
uint8_t LTC2944_I2C_ReadByte(uint8_t addr)
{
    while(I2C_GetFlagStatus(LTC2944_I2C, I2C_FLAG_BUSY));

    I2C_GenerateSTART(LTC2944_I2C, ENABLE);//开启I2C1
    while(!I2C_CheckEvent(LTC2944_I2C, I2C_EVENT_MASTER_MODE_SELECT));/*EV5,主模式*/

    I2C_Send7bitAddress(LTC2944_I2C, 0xC8, I2C_Direction_Transmitter);//器件地址0xC8
    while(!I2C_CheckEvent(LTC2944_I2C, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(LTC2944_I2C, addr);//寄存器地址
    while (!I2C_CheckEvent(LTC2944_I2C, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTART(LTC2944_I2C, ENABLE);//开启I2C1
    while(!I2C_CheckEvent(LTC2944_I2C, I2C_EVENT_MASTER_MODE_SELECT));/*EV5,主模式*/

    I2C_Send7bitAddress(LTC2944_I2C, 0xC8, I2C_Direction_Receiver);//器件地址 0xc8
    while(!I2C_CheckEvent(LTC2944_I2C, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

    I2C_AcknowledgeConfig(LTC2944_I2C, DISABLE);
    I2C_GenerateSTOP(LTC2944_I2C,ENABLE);

    while (!I2C_CheckEvent(LTC2944_I2C, I2C_EVENT_MASTER_BYTE_RECEIVED));
    uint8_t uData = I2C_ReceiveData(LTC2944_I2C);

    I2C_AcknowledgeConfig(LTC2944_I2C, ENABLE);
    return uData;
}
