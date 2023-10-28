#include "ch32v30x.h"
#include <rtthread.h>
#include <rthw.h>
#include "drivers/pin.h"
#include <board.h>
#include <math.h>

#include "Peripheral_Init.h"
#include "A9G.h"
#include "my_printf.h"
#include "OLED.h"
#include "thermistor.h"
#include "data_show.h"
#include "led.h"
#include "Before_Init.h"
#include "common_setting.h"
#include "QMA7981.h"
#include "LTC2944.h"
#include "breakage_alarm.h"

static struct rt_semaphore OLED_Button_Click_Sem;//OLED按键点击信号量

rt_uint32_t uEventVal_OLED;//OLED事件的值

/**
  * @brief OLED按键中断回调函数
  */
void OLED_Button_Click_Callback(void *args)
{
    rt_sem_release(&OLED_Button_Click_Sem);
}

/**
  * @brief OLED按键检测线程入口函数
  */
void OLED_Button_Scan_Entry(void* parameter)
{
    static rt_uint8_t Click_Count = 1;//按键点击计数

    /* 初始化清屏 */
    Clear_Part_Screen(0, 52, 2, 76);
    Clear_Part_Screen(2, 0, 8, 128);

    /* A9G初始化界面显示 */
    OLED_ShowString(2, 7, "A9G&MQTT Init", 16);
    OLED_DrawBMP(4, 0, 2, 128, Progress_Bar);
    OLED_ShowString(6, 20, "Loding:", 16);
    OLED_ShowChar(6, 100, '%', 16);
    if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                    (A9G_Init_Refresh_OK_Event | A9G_Init_Refresh_Error_Event),
                     RT_EVENT_FLAG_OR,
                     RT_WAITING_FOREVER,
                     &uEventVal_OLED) == RT_EOK)
    {
        /* 成功显示100%后退出初始化界面 */
        if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                         A9G_Init_Refresh_OK_Event,
                        (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                         RT_WAITING_NO,
                         &uEventVal_OLED) == RT_EOK)
        {
            A9G_Init_Progress_Bar_Show(100);
            rt_event_send(&OLED_Data_Refresh_EventSet, Signal_Refresh_OK_Event);
        }
        /* 失败显示50%后退出 */
        if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                         A9G_Init_Refresh_Error_Event,
                        (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                         RT_WAITING_NO,
                         &uEventVal_OLED) == RT_EOK)
        {
            A9G_Init_Progress_Bar_Show(40);
            OLED_ShowString(6, 24, "Init Error", 16);
        }
    }

    /* 设置OLED按键引脚为输入模式 */
    rt_pin_mode(OLED_Button_PIN, PIN_MODE_INPUT_PULLUP);

    /* 绑定OLED按键中断，下降沿模式，回调函数名为beep_on */
    rt_pin_attach_irq(OLED_Button_PIN, PIN_IRQ_MODE_FALLING, OLED_Button_Click_Callback, RT_NULL);

    /* 使能OLED按键中断 */
    rt_pin_irq_enable(OLED_Button_PIN, PIN_IRQ_ENABLE);

    /* 清屏 */
    Clear_Part_Screen(0, 52, 2, 76);
    Clear_Part_Screen(2, 0, 8, 128);

    /* 封面显示 */
    Cover_Interface_Show();

    /* 开始响应OLED按键 */
    while(1)
    {
        /* 阻塞方式等待OLED按键点击信号量 */
        rt_sem_take(&OLED_Button_Click_Sem, RT_WAITING_FOREVER);

        /* OLED按键消抖 */
        rt_thread_mdelay(150);
        while(1)
        {
            /* 消耗所有OLED按键点击信号量 */
            if(rt_sem_take(&OLED_Button_Click_Sem, RT_WAITING_NO) != RT_EOK)
                break;
        }

        /* 根据用户点击按键选择显示功能 */
        switch(Click_Count)
        {
        case 0:
            Clear_Part_Screen(0, 52, 2, 76);
            Clear_Part_Screen(2, 0, 8, 128);

            rt_event_send(&OLED_Data_Refresh_EventSet, Location_End_Show_Event);

            Cover_Interface_Show();
            break;
        case 1:
            Clear_Part_Screen(0, 52, 2, 76);
            Clear_Part_Screen(2, 0, 8, 128);

            rt_event_send(&OLED_Data_Refresh_EventSet, Temperature_Show_Event);

            OLED_DrawBMP(0, 52, 2, 24, Thermometer_Mark);
            OLED_DrawBMP(2, 104, 6, 24, Temperature_Sign);
            break;
        case 2:
            Clear_Part_Screen(0, 52, 2, 76);
            Clear_Part_Screen(2, 0, 8, 128);

            rt_event_send(&OLED_Data_Refresh_EventSet, Temperature_End_Show_Event | Movement_Show_Event);

            OLED_DrawBMP(0, 52, 2, 24, Movement_Mark);
            OLED_ShowString(2, 94, "Step", 16);
            OLED_DrawBMP(4, 0, 2, 128, Progress_Bar);
            OLED_ShowString(6, 24, "Reach:", 16);
            OLED_ShowChar(6, 96, '%', 16);
            break;
        case 3:
            Clear_Part_Screen(0, 52, 2, 76);
            Clear_Part_Screen(2, 0, 8, 128);

            rt_event_send(&OLED_Data_Refresh_EventSet, Movement_End_Show_Event | Location_Show_Event);

            OLED_DrawBMP(0, 52, 2, 24, Fence_Mark);
            OLED_ShowString(2, 0, "###", 16);
            OLED_ShowString(4, 0, "###", 16);
            OLED_ShowString(6, 0, "###", 16);
            OLED_DrawBMP(2, 72, 6, 48, Big_Fence);
            break;
        }
        Click_Count ++;
        if(Click_Count > 3)
            Click_Count = 0;
    }
}

/**
  * @brief OLED数据刷新线程入口函数
  */
void OLED_Data_Refresh_Entry(void* parameter)
{
    static uint8_t uMovement_Progress = 0;//运动量进度条数值
    /* 接收到相应的OLED显示事件后开启相应显示 */
    while(1)
    {
        /* 阻塞方式等待显示事件 */
        if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                        (Temperature_Show_Event | Movement_Show_Event | Location_Show_Event),
                         RT_EVENT_FLAG_OR,
                         RT_WAITING_FOREVER,
                         &uEventVal_OLED) == RT_EOK)
        {
            /* 温度显示部分 */
            if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                             Temperature_Show_Event,
                            (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                             RT_WAITING_NO,
                             &uEventVal_OLED) == RT_EOK)
            {
                while(1)
                {
                    if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                                    (Temperature_Refresh_Event | Temperature_End_Show_Event),
                                     RT_EVENT_FLAG_OR,
                                     RT_WAITING_FOREVER,
                                     &uEventVal_OLED) == RT_EOK)
                    {

                        if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                                         Temperature_End_Show_Event,
                                        (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                                         RT_WAITING_NO,
                                         &uEventVal_OLED) == RT_EOK)
                        {
                            break;
                        }

                        if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                                         Temperature_Refresh_Event,
                                        (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                                         RT_WAITING_NO,
                                         &uEventVal_OLED) == RT_EOK)
                        {
                            OLED_ShowFloat(2, 0, Temp_Value, 1, 48);
                        }
                    }
                }
            continue;
            }

            /* 运动量显示部分 */
            if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                             Movement_Show_Event,
                            (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                             RT_WAITING_NO,
                             &uEventVal_OLED) == RT_EOK)
            {
                OLED_ShowInt(2, 0, uStep_Sum, 16);
                Movement_Progress_Bar_Show(uMovement_Progress);
                OLED_ShowInt(6, 72, uMovement_Progress, 16);

                while(1)
                {
                    if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                                    (Movement_Refresh_Event | Movement_End_Show_Event),
                                     RT_EVENT_FLAG_OR,
                                     RT_WAITING_FOREVER,
                                     &uEventVal_OLED) == RT_EOK)
                    {
                        if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                                         Movement_End_Show_Event,
                                        (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                                         RT_WAITING_NO,
                                         &uEventVal_OLED) == RT_EOK)
                        {
                            break;
                        }

                        if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                                         Movement_Refresh_Event,
                                        (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                                         RT_WAITING_NO,
                                         &uEventVal_OLED) == RT_EOK)
                        {
                            OLED_ShowInt(2, 0, uStep_Sum, 16);
                            uMovement_Progress = (uint8_t)(uStep_Sum / 10);
                            Movement_Progress_Bar_Show(uMovement_Progress);
                            OLED_ShowInt(6, 72, uMovement_Progress, 16);
                        }
                    }
                }
            continue;
            }

            /* 定位以及电子围栏显示部分 */
            if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                             Location_Show_Event,
                            (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                             RT_WAITING_NO,
                             &uEventVal_OLED) == RT_EOK)
            {
                while(1)
                {
                    if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                                    (Location_Refresh_OK_Event | Location_End_Show_Event),
                                     RT_EVENT_FLAG_OR,
                                     RT_WAITING_FOREVER,
                                     &uEventVal_OLED) == RT_EOK)
                    {

                        if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                                         Location_End_Show_Event,
                                        (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                                         RT_WAITING_NO,
                                         &uEventVal_OLED) == RT_EOK)
                        {
                            break;
                        }

                        if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                                         Location_Refresh_OK_Event,
                                        (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                                         RT_WAITING_NO,
                                         &uEventVal_OLED) == RT_EOK)
                        {
                            /* 电子围栏显示 */
                            Electronic_Fence_Show(24.623059, 118.083546, 35, fLati_Val, fLongi_Val);
                        }
                    }
                }
            continue;
            }
        }
    }
}

/**
  * @brief OLED顶部状态栏刷新线程入口函数
  */
void Status_Bar_Refresh_Entry(void* parameter)
{
    float Before_Battery_Percent = 100.0;//之前电池电量记录

    /* 电池电量显示 */
    OLED_DrawBMP(0, 0, 2, 24, No_Signal_Mark);
    OLED_DrawBMP(0, 104, 2, 24, Battery_Mark);
    Battery_Percent_Bar_Show(0.0, Before_Battery_Percent);

    /* 根据相应事件更新顶部状态栏显示 */
    while(1)
    {
        if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                        (Battery_Refresh_Event | Signal_Refresh_OK_Event),
                         RT_EVENT_FLAG_OR,
                         RT_WAITING_FOREVER,
                         &uEventVal_OLED) == RT_EOK)
        {
            /* 电池电量显示部分 */
            if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                             Battery_Refresh_Event,
                            (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                             RT_WAITING_NO,
                             &uEventVal_OLED) == RT_EOK)
            {
                Battery_Percent_Bar_Show(Before_Battery_Percent, Battery_Percent);
                Before_Battery_Percent = Battery_Percent;
            }

            /* 信号显示部分 */
            if(rt_event_recv(&OLED_Data_Refresh_EventSet,
                             Signal_Refresh_OK_Event,
                            (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                             RT_WAITING_NO,
                             &uEventVal_OLED) == RT_EOK)
            {
                OLED_DrawBMP(0, 0, 2, 24, Get_Signal_Mark);
            }
        }
    }
}

/**
  * @brief OLED显示全局开启
  */
int OLED_And_Button_Start(void)
{
    static rt_thread_t Status_Bar_Refresh_Thread;//定义状态栏显示线程
    static rt_thread_t Button_Scan_Thread;//定义OLED按键检测线程
    static rt_thread_t OLED_Data_Refresh_Thread;//定义OLED数据显示更新线程

    /* 初始化用户按键点击信号量 */
    rt_sem_init(&OLED_Button_Click_Sem, "OLED_Button_Click_Sem_Name", 0, RT_IPC_FLAG_FIFO);

    /* 动态创建并开启状态栏显示线程 */
    Status_Bar_Refresh_Thread = rt_thread_create("Status_Bar_Refresh_Thread_Name",
                                                 Status_Bar_Refresh_Entry,
                                                 RT_NULL,
                                                 STACK_SIZE_1024,
                                                 PRIORITY_14,
                                                 TIMESLICE_5);
    if(Status_Bar_Refresh_Thread != RT_NULL)
        rt_thread_startup(Status_Bar_Refresh_Thread);

    /* 动态创建并开启OLED按键检测线程 */
    Button_Scan_Thread = rt_thread_create("Button_Scan_Thread_Name",
                                          OLED_Button_Scan_Entry,
                                          RT_NULL,
                                          STACK_SIZE_1024,
                                          PRIORITY_15,
                                          TIMESLICE_5);
    if(Button_Scan_Thread != RT_NULL)
        rt_thread_startup(Button_Scan_Thread);

    /* 动态创建并开启OLED数据显示更新线程 */
    OLED_Data_Refresh_Thread = rt_thread_create("OLED_Data_Refresh_Thread_Name",
                                                OLED_Data_Refresh_Entry,
                                                RT_NULL,
                                                STACK_SIZE_1024,
                                                PRIORITY_16,
                                                TIMESLICE_5);
    if(OLED_Data_Refresh_Thread != RT_NULL)
        rt_thread_startup(OLED_Data_Refresh_Thread);
    return 0;
}

/**
  * @brief 清除OLED特定区域
  * @param Y0 , Y1 行（0--7）
  * @param X0 , X1 列（0--127）
  */
void Clear_Part_Screen(uint8_t Y0, uint8_t X0, uint8_t Y1, uint8_t X1)
{
    uint8_t uY_Cnt = Y0, uX_Cnt = X0;
    for(uY_Cnt = Y0; uY_Cnt < Y1; uY_Cnt++)
    {
        OLED_SetCursor(uY_Cnt, X0);
        for(uX_Cnt = X0; uX_Cnt < X1; uX_Cnt++)
        {
            OLED_WriteData(0x00);
        }
    }
}

/**
  * @brief 进度条内容显示
  * @param (x,y) 进度条原点
  * @param Now_Step 进度条现在的格数
  * @param Next_Step 下一步到达的格位
  */
void Rate_Bar_Show(uint8_t Y, uint8_t X, uint8_t Now_Step, uint8_t Bar_Height)
{
    OLED_SetCursor(Y, X);
    X -= 14;
    while(X < Now_Step)
    {
        OLED_WriteData(Bar_Height);
        X ++;
    }
}

/**
  * @brief OLED封面显示
  */
void Cover_Interface_Show(void)
{
    OLED_DrawBMP(2, 0, 6, 52, Cow_Mark);
    OLED_ShowString(2, 68, "Smart", 16);
    OLED_ShowString(4, 65, "Animal", 16);
    OLED_ShowString(6, 52, "Neck Ring", 16);
}

/**
  * @brief OLED显示A9G初始化进度条
  * @param Now_Step 现在的格数
  */
void A9G_Init_Progress_Bar_Show(uint8_t Now_Step)
{
    uint8_t uBefore_Step = 0;

    for(uBefore_Step = 0; (uBefore_Step + 1) <= Now_Step; uBefore_Step ++)
    {
        OLED_ShowInt(6, 76, uBefore_Step + 1, 16);
        Rate_Bar_Show(4, 14 + uBefore_Step, (uBefore_Step + 1), 0xFC);
        Rate_Bar_Show(5, 14 + uBefore_Step, (uBefore_Step + 1), 0x3F);
    }
}

/**
  * @brief OLED显示运动量进度条
  * @param Now_Step 现在的格数
  */
void Movement_Progress_Bar_Show(uint8_t Now_Step)
{
    Rate_Bar_Show(4, 14, Now_Step, 0xFC);
    Rate_Bar_Show(5, 14, Now_Step, 0x3F);
}

/**
  * @brief OLED显示电池电量
  * @param Before_Percent 前电池电量百分比
  * @param Now_Percent 现在电池电量百分比
  */
void Battery_Percent_Bar_Show(float Before_Percent, float Now_Percent)
{
    uint8_t uBefore_Val = (uint8_t)(Before_Percent / 5);
    uint8_t uNow_Val = (uint8_t)(Now_Percent / 5);
    if(uBefore_Val > uNow_Val)
    {
        while(uBefore_Val > uNow_Val)
        {
            uBefore_Val --;
            OLED_SetCursor(0, (106 + uBefore_Val));
            OLED_WriteData(0x04);
            OLED_SetCursor(1, (106 + uBefore_Val));
            OLED_WriteData(0x20);
        }
    }
    if(uBefore_Val < uNow_Val)
    {
        while(uBefore_Val < uNow_Val)
        {
            OLED_SetCursor(0, (106 + uBefore_Val));
            OLED_WriteData(0xFC);
            OLED_SetCursor(1, (106 + uBefore_Val));
            OLED_WriteData(0x3F);
            uBefore_Val ++;
        }
    }
}

/**
  * @brief OLED电子围栏显示以及阿里云地理位置状态上报
  * @param Origin_Lati , Origin_Longi 电子围栏中心的纬度和经度
  * @param Range_R 电子围栏范围
  * @param Lati , Longi 目标的地理位置
  */
void Electronic_Fence_Show(double Origin_Lati, double Origin_Longi,double Range_R, double Lati, double Longi)
{
    double Distance_Val = Distance(Origin_Lati, Origin_Longi, Lati, Longi);//目标距离电子围栏中心点距离
    uint8_t uAngle = Orientation(Origin_Lati, Origin_Longi, Lati, Longi);//目标与电子围栏中心方位
    double Gap = Range_R - Distance_Val;//目标与围栏距离

    /* 距离中心数据显示 */
    OLED_ShowFloat(2, 0, Distance_Val, 1, 16);
    OLED_ShowChar(2, 32, 'm', 16);

    /* 距离安全显示 */
    if(Gap >= Safe_Distance)
    {
        OLED_ShowString(6, 0, "Safe", 16);
        OLED_DrawBMP(2, 72, 6, 48, Big_Fence);

        /* 如果目标位置靠近中心则OLED目标点显示在中心 */
        if(Distance_Val < Centre_Distance)
            OLED_DrawBMP(4, 94, 2, 4, Location_Point_Centre_And_Out_Left_Right);

        /* 地理位置状态上报阿里云 */
        rt_mutex_take(A9G_Occupy_Mutex, RT_WAITING_FOREVER);
        my_printf("AT+MQTTPUB=\"/sys/in32QSarJUf/Device_01/thing/event/property/post\",\"{\\22id\\22:\\22123\\22,\\22version\\22:\\221.0\\22,\\22params\\22:{\\22Location_State\\22:\\22%s\\22},\\22method\\22:\\22thing.event.property.post\\22}\",0,0,0\r\n",
                "In_Fence");//上传数据到阿里云
        rt_thread_mdelay(1000);
        rt_device_write(A9G_UART_Serial, 0, "AT+GPS=?\r\n", sizeof("AT+GPS=?\r\n"));
        rt_event_recv(&A9G_EventSet,
                      OK_FeedBack_Event,
                     (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                      RT_WAITING_FOREVER,
                      &uEventVal_A9G);
        rt_mutex_release(A9G_Occupy_Mutex);
    }

    /* 距离危险显示 */
    else if((Safe_Distance > Gap) && (Gap > 0.0))
    {
        OLED_ShowString(6, 0, "Warning", 16);
        OLED_DrawBMP(2, 72, 6, 48, Big_Fence);

        /* 地理位置状态上报阿里云 */
        rt_mutex_take(A9G_Occupy_Mutex, RT_WAITING_FOREVER);
        my_printf("AT+MQTTPUB=\"/sys/in32QSarJUf/Device_01/thing/event/property/post\",\"{\\22id\\22:\\22123\\22,\\22version\\22:\\221.0\\22,\\22params\\22:{\\22Location_State\\22:\\22%s\\22},\\22method\\22:\\22thing.event.property.post\\22}\",0,0,0\r\n",
                "Near_Fence");//上传数据到阿里云
        rt_thread_mdelay(1000);
        rt_device_write(A9G_UART_Serial, 0, "AT+GPS=?\r\n", sizeof("AT+GPS=?\r\n"));
        rt_event_recv(&A9G_EventSet,
                      OK_FeedBack_Event,
                     (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                      RT_WAITING_FOREVER,
                      &uEventVal_A9G);
        rt_mutex_release(A9G_Occupy_Mutex);
    }

    /* 目标出围栏显示 */
    else
    {
        OLED_ShowString(6, 0, "Out", 16);
        OLED_DrawBMP(2, 72, 6, 48, Small_Fence);

        /* 地理位置状态上报阿里云并且通过电话和短信提示 */
        rt_mutex_take(A9G_Occupy_Mutex, RT_WAITING_FOREVER);
        /* 电话提示 */
        rt_device_write(A9G_UART_Serial, 0, "ATD15860597877\r\n", sizeof("ATD15860597877\r\n"));
        rt_thread_mdelay(500);
        /* 短信提示 */
        rt_device_write(A9G_UART_Serial, 0, "AT+CMGF=1\r\n", sizeof("AT+CMGF=1\r\n"));
        rt_thread_mdelay(500);
        rt_device_write(A9G_UART_Serial, 0, "AT+CMGS=\"15860597877\"\r\n", sizeof("AT+CMGS=\"15860597877\"\r\n"));
        rt_thread_mdelay(500);
        rt_device_write(A9G_UART_Serial, 0, "The Animal Is Out Of Fence\r\n", sizeof("The Animal Is Out Of Fence\r\n"));
        rt_thread_mdelay(500);
        rt_device_write(A9G_UART_Serial, 0, &Ctrl_Z, 1);
        rt_thread_mdelay(500);
        /* 地理位置状态上报阿里云 */
        my_printf("AT+MQTTPUB=\"/sys/in32QSarJUf/Device_01/thing/event/property/post\",\"{\\22id\\22:\\22123\\22,\\22version\\22:\\221.0\\22,\\22params\\22:{\\22Location_State\\22:\\22%s\\22},\\22method\\22:\\22thing.event.property.post\\22}\",0,0,0\r\n",
                "Out_Fence");
        rt_thread_mdelay(1000);
        rt_device_write(A9G_UART_Serial, 0, "AT+GPS=?\r\n", sizeof("AT+GPS=?\r\n"));
        rt_event_recv(&A9G_EventSet,
                      OK_FeedBack_Event,
                     (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                      RT_WAITING_FOREVER,
                      &uEventVal_A9G);
        rt_mutex_release(A9G_Occupy_Mutex);
    }

    /* 方位以及距离显示 */
    if(Distance_Val != 0)
    {
        if((uAngle == 0) | (uAngle == 360))
        {
            OLED_ShowString(4, 0, "North", 16);
            if(Gap >= Safe_Distance)
                OLED_DrawBMP(3, 94, 2, 4, Location_Point_In_Top);//内顶
            else
                OLED_DrawBMP(2, 94, 1, 4, Location_Point_Out_Top);//外顶
        }
        else if(uAngle == 90)
        {
            OLED_ShowString(4, 0, "East", 16);
            if(Gap >= Safe_Distance)
                OLED_DrawBMP(4, 102, 2, 4, Location_Point_In_Right);//内右
            else
                OLED_DrawBMP(4, 112, 2, 4, Location_Point_Centre_And_Out_Left_Right);//外右
        }
        else if(uAngle == 180)
        {
            OLED_ShowString(4, 0, "South", 16);
            if(Gap >= Safe_Distance)
                OLED_DrawBMP(5, 94, 2, 4, Location_Point_In_Bottom);//内底
            else
                OLED_DrawBMP(7, 94, 1, 4, Location_Point_Out_Bottom);//外底
        }
        else if(uAngle == 270)
        {
            OLED_ShowString(4, 0, "West", 16);
            if(Gap >= Safe_Distance)
                OLED_DrawBMP(4, 86, 2, 4, Location_Point_In_Left);//内左
            else
                OLED_DrawBMP(4, 75, 2, 4, Location_Point_Centre_And_Out_Left_Right);//外左
        }
        else if((uAngle > 0) && (uAngle < 90))
        {
            OLED_ShowString(4, 0, "NtoE:", 16);
            OLED_ShowInt(4, 40, uAngle, 16);
            OLED_DrawBMP(4, 56, 2, 8, Du_Sign);
            if(Gap >= Safe_Distance)
                OLED_DrawBMP(3, 100, 2, 4, Location_Point_In_Right_Up);//内右上
            else
                OLED_DrawBMP(3, 108, 1, 4, Location_Point_Out_Right_Up);//外右上
        }
        else if((uAngle > 90) && (uAngle < 180))
        {
            OLED_ShowString(4, 0, "EtoS:", 16);
            OLED_ShowInt(4, 40, (uAngle - 90), 16);
            OLED_DrawBMP(4, 56, 2, 8, Du_Sign);
            if(Gap >= Safe_Distance)
                OLED_DrawBMP(5, 100, 2, 4, Location_Point_In_Right_Down);//内右下
            else
                OLED_DrawBMP(6, 108, 1, 4, Location_Point_Out_Right_Down);//外右下
        }
        else if((uAngle > 180) && (uAngle < 270))
        {
            OLED_ShowString(4, 0, "WtoS:", 16);
            OLED_ShowInt(4, 40, (270 - uAngle), 16);
            OLED_DrawBMP(4, 56, 2, 8, Du_Sign);
            if(Gap >= Safe_Distance)
                OLED_DrawBMP(5, 88, 2, 4, Location_Point_In_Left_Down);//内左下
            else
                OLED_DrawBMP(6, 80, 1, 4, Location_Point_Out_Left_Down);//外左下
        }
        else if((uAngle > 270) && (uAngle < 360))
        {
            OLED_ShowString(4, 0, "NtoW:", 16);
            OLED_ShowInt(4, 40, (uAngle - 270), 16);
            OLED_DrawBMP(4, 56, 2, 8, Du_Sign);
            if(Gap >= Safe_Distance)
                OLED_DrawBMP(3, 88, 2, 4, Location_Point_In_Left_Up);//内左上
            else
                OLED_DrawBMP(3, 80, 1, 4, Location_Point_Out_Left_Up);//外左上
        }
    }
}

/**
  * @brief 两点之间的距离计算
  * @param Origin_Lati , Origin_Longi 原点的纬度和经度
  * @param Lati , Longi 目标的纬度和经度
  */
double Distance(double Origin_Lati, double Origin_Longi, double Lati, double Longi)
{
    double Per_Ang_To_Rad = M_PI / 180;

    uint32_t ra = 6378140;  // 赤道半径
    uint32_t rb = 6356755;  //极半径
    double flatten = (ra - rb) / ra;  // Partial rate of the earth
    /* change angle to radians */
    double rad_Lati = Lati * Per_Ang_To_Rad;
    double rad_Longi = Longi * Per_Ang_To_Rad;

    double rad_Origin_Lati = Origin_Lati * Per_Ang_To_Rad;
    double rad_Origin_Longi = Origin_Longi * Per_Ang_To_Rad;

    double p = atan(rb / ra * tan(rad_Lati));
    double p_Origin = atan(rb / ra * tan(rad_Origin_Lati));
    double x = acos(sin(p) * sin(p_Origin) + cos(p) * cos(p_Origin) * cos(rad_Longi - rad_Origin_Longi));
    double c1 = pow((sin(x) - x) * pow((sin(p) + sin(p_Origin)), 2)/ cos(x / 2), 2);
    double c2 = pow((sin(x) + x) * pow((sin(p) - sin(p_Origin)), 2) / sin(x / 2), 2);
    double dr = flatten / 8 * (c1 - c2);
    double distance = (ra * (x + dr) );// / 1000

    return distance;
}

/**
  * @brief 两点之间的方位计算
  * @param Origin_Lati , Origin_Longi 原点的纬度和经度
  * @param Lati , Longi 目标的纬度和经度
  */
int Orientation(double Origin_Lati, double Origin_Longi, double Lati, double Longi)
{
    double Per_Ang_To_Rad = M_PI / 180;
    double Per_Rad_To_Ang = 180 / M_PI;

    double rad_Origin_Lati = Origin_Lati * Per_Ang_To_Rad;
    double rad_Origin_Longi = Origin_Longi * Per_Ang_To_Rad;

    double rad_Lati = Lati * Per_Ang_To_Rad;
    double rad_Longi = Longi * Per_Ang_To_Rad;

    double dLon = rad_Longi - rad_Origin_Longi;
    double y = sin(dLon) * cos(rad_Lati);
    double x = cos(rad_Origin_Lati) * sin(rad_Lati) - sin(rad_Origin_Lati) * cos(rad_Lati) * cos(dLon);
    double brng = atan2(y, x) * Per_Rad_To_Ang;
    int Angle = ((int)brng + 360) % 360;

    return Angle;
}
