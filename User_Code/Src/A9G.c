#include <rtthread.h>

#include <rtdbg.h>
#include <board.h>
#include <rtdevice.h>
#include <string.h>
#include <drivers/pin.h>
#include <stdlib.h>
#include <math.h>

#include "A9G.h"
#include "led.h"
#include "my_printf.h"
#include "common_setting.h"
#include "OLED.h"
#include "Before_Init.h"
#include "data_show.h"

char aA9G_UART_RxBuffer[RxBuffer_MAX];//储存一段完整的A9G反馈信息

/* 用于接收消息的信号量 */
static struct rt_semaphore A9G_UART_Rx_Sem;

/* 串口设备句柄 */
rt_device_t A9G_UART_Serial;
rt_device_t Interaction_UART_Serial;

/* A9G初始化反馈比较判断字符串 */
char aOK[] = "OK";
char aOverFlow[] = "数据溢出";

/* 转化后的浮点型经纬度数据 */
double fLati_Val = 0.0 ;//纬度
double fLongi_Val = 0.0;//经度

/*
 * 接收到数据回调函数
 * */
static rt_err_t A9G_Rx_CallBack(rt_device_t dev, rt_size_t size)
{
    /* 串口接收到数据后产生中断，调用此回调函数，然后发送接收信号量 */
    rt_sem_release(&A9G_UART_Rx_Sem);
    return RT_EOK;
}

/**
  * @brief A9G串口接收线程入口函数
  * */
static void A9G_UART_Rx_Entry(void *parameter)
{
    rt_uint8_t uLocation_State = 0;//是否接收定位数据判断标志位
    rt_uint8_t uA9G_Roll_RxBuffer;//储存一个字节的A9G反馈信息

    static char aFind_OK[2];//储存定位判断到的OK字符串，无static修饰为静态变量会在接下来的数据传递出现错误
    static char aInit_Error[] = "\n+CME ERROR: 3\r\n\0";//AT+CGDCONT=1,\"IP\",\"cmnet\"\r\n\r\n+CME ERROR: 3\r\n\0
    static char aLocation_Error[] = "AT+LOCATION=2\r\n+";//完整反馈：AT+LOCATION=2\r\n+LOCATION: GPS NOT FIX NOW\r\n+CME ERROR: 52\r\n\0
    static char aLocation_OK[] = "AT+LOCATION=2\r\n\r";

    rt_uint8_t uA9G_UART_Rx_Cnt = 0;//A9G串口接收字节数计数

    while (1)
    {
        /* 阻塞方式等待A9G接收信号量 */
        rt_sem_take(&A9G_UART_Rx_Sem, RT_WAITING_FOREVER);

        /* 串口接收数据并转存 */
        rt_device_read(A9G_UART_Serial, -1, &uA9G_Roll_RxBuffer, 1);
        aA9G_UART_RxBuffer[uA9G_UART_Rx_Cnt] = uA9G_Roll_RxBuffer;

        /* 判断接收的数据量是否溢出 */
        if(uA9G_UART_Rx_Cnt >= RxBuffer_MAX)
        {
            uA9G_UART_Rx_Cnt = -1;
            memset(aA9G_UART_RxBuffer,0x00,RxBuffer_MAX);
            rt_kprintf("%s",aOverFlow);
        }

        /* 判断接收是否结束 */
        if((aA9G_UART_RxBuffer[uA9G_UART_Rx_Cnt - 1] == 0x0A)&&(aA9G_UART_RxBuffer[uA9G_UART_Rx_Cnt-2] == 0x0D))//0x00 = \0，0x0A = \n，0x0D = \r
        {
            /* 接收结束闪烁一次 */
            LED_Twinkle(LED0, 250, 1);

            /* 寻找接收的字符串中的“OK”两个字符 */
            aFind_OK[0] = aA9G_UART_RxBuffer[uA9G_UART_Rx_Cnt - 4];
            aFind_OK[1] = aA9G_UART_RxBuffer[uA9G_UART_Rx_Cnt - 3];

            /* 判断是否开始接收定位数据 */
            if(uLocation_State == Location_Start)
            {
                LED_Twinkle(LED1, 250, 1);

                Location_To_Float((char*)&aA9G_UART_RxBuffer);
                rt_kprintf("lati is %d\r\n",(uint8_t)fLati_Val);
                rt_kprintf("longi is %d\r\n",(uint8_t)fLongi_Val);
                rt_event_send(&OLED_Data_Refresh_EventSet, Location_Refresh_OK_Event);

                uLocation_State = Location_Close;
            }

            /* 判断定位是否出错 */
            if(strcmp((char*)&aA9G_UART_RxBuffer, (char*)&aLocation_Error) == 0)
            {
                LED_Twinkle(LED1, 250, 1);
                rt_event_send(&A9G_EventSet, Location_Error_Event);
            }
            if(strcmp((char*)&aA9G_UART_RxBuffer, (char*)&aLocation_OK) == 0)
            {
                uLocation_State = Location_Start;
            }

            /* 判断A9G初始化是否出错 */
            if(strcmp((char*)&aA9G_UART_RxBuffer, (char*)&aInit_Error) == 0)
            {
                rt_event_send(&A9G_EventSet, A9G_Init_Error_Event);
                rt_event_send(&OLED_Data_Refresh_EventSet, A9G_Init_Refresh_Error_Event);
            }

            /* 判断对A9G的操作是否成功，成功则标志事件位并处理接收到的数据 */
            if(strcmp((char*)&aFind_OK, (char*)&aOK) == 0)
            {
                rt_event_send(&A9G_EventSet, OK_FeedBack_Event);
            }

            /* 打印接收到的数据，并初始化接收数组和计数 */
            rt_kprintf("%s",aA9G_UART_RxBuffer);
            memset(aA9G_UART_RxBuffer,0x00,RxBuffer_MAX);
            uA9G_UART_Rx_Cnt = -1;//-1 ++ = 0
        }
        uA9G_UART_Rx_Cnt++;
    }
}

/*
 * A9G初始化操作发送入口函数
 * */
void A9G_Init_Ops_Send_Entry(void* parameter)
{
    rt_uint8_t Init_Step_Cnt = 0;//初始化步骤记录

    Device_Property Device_Prop = {
                "in32QSarJUf",
                "Device_01",
                "8a72d9ae83dae2409e393e6bab08c40a",
                "12345",
                "3B8F8097283EDBB4D2CBE0CC0E400312"};//阿里云连接参数

    /* 占用互斥量，防止A9G数据发送阿里云的通道被占用 */
    rt_mutex_take(A9G_Occupy_Mutex, RT_WAITING_FOREVER);

    /* 初始化A9G事件集的每个标志位 */
    A9G_EventSet.set &= 0x00000000;

    /* A9G各个初始化步骤，每步初始化都需要一定的事件激活 */
    rt_device_write(A9G_UART_Serial, 0, "AT+RST\r\n", sizeof("AT+RST\r\n"));
    while(1)
    {
        if(Init_Step_Cnt == 5)
            break;
        if(rt_event_recv(&A9G_EventSet,
                        (OK_FeedBack_Event | A9G_Init_Error_Event),
                         RT_EVENT_FLAG_OR,
                         RT_WAITING_FOREVER,
                         &uEventVal_A9G) == RT_EOK)
        {
            if(rt_event_recv(&A9G_EventSet,
                             OK_FeedBack_Event,
                            (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                             RT_WAITING_NO,
                             &uEventVal_A9G) == RT_EOK)
            {
                switch(Init_Step_Cnt)
                {
                case 0:
                    rt_device_write(A9G_UART_Serial, 0, "AT+CGATT=1\r\n", sizeof("AT+CGATT=1\r\n"));
                    break;
                case 1:
                    rt_device_write(A9G_UART_Serial, 0, "AT+CGDCONT=1,\"IP\",\"cmnet\"\r\n", sizeof("AT+CGDCONT=1,\"IP\",\"cmnet\"\r\n"));
                    break;
                case 2:
                    rt_device_write(A9G_UART_Serial, 0, "AT+CGACT=1,1\r\n", sizeof("AT+CGACT=1,1\r\n"));
                    break;
                case 3:
                    /* 通过MQTT连接阿里云 */
                    my_printf("AT+MQTTCONN=\"%s.iot-as-mqtt.cn-shanghai.aliyuncs.com\",1883,\"%s|securemode=3,signmethod=hmacmd5|\",120,0,\"%s&%s\",\"%s\"\r\n",
                            Device_Prop.ProductKey,
                            Device_Prop.clientId,
                            Device_Prop.DeviceName,
                            Device_Prop.ProductKey,
                            Device_Prop.password);
                    rt_thread_mdelay(1000);

                    /* 订阅物模型主题 */
                    my_printf("AT+MQTTSUB=\"/sys/%s/%s/thing/service/property/set\",1,0\r\n",
                            Device_Prop.ProductKey,
                            Device_Prop.DeviceName);
                    rt_thread_mdelay(1000);

                    /* 发送定位状态询问消息，目的只是为了解决发送数据后反馈消息接收不完全的问题 */
                    rt_device_write(A9G_UART_Serial, 0, "AT+GPS=?\r\n", sizeof("AT+GPS=?\r\n"));
                    break;
                case 4:
                    /* 发送OLED刷新事件，显示A9G初始化成功 */
                    rt_event_send(&OLED_Data_Refresh_EventSet, A9G_Init_Refresh_OK_Event);

                    /* 释放互斥量，此时别的功能可占用A9G模块 */
                    rt_mutex_release(A9G_Occupy_Mutex);
                    break;
                }
                Init_Step_Cnt ++;
            }
            if(rt_event_recv(&A9G_EventSet,
                             A9G_Init_Error_Event,
                            (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                             RT_WAITING_NO,
                             &uEventVal_A9G) == RT_EOK)
            {
                /* 释放互斥量，此时别的功能可占用A9G模块 */
                rt_mutex_release(A9G_Occupy_Mutex);
                break;
            }
        }
    }
}


/*
 *  获取定位数据入口函数
 * */
void Get_Location_Entry(void* parameter)
{
    rt_uint8_t Location_Step_Cnt = 0;//定位步骤记录

    /* 占用互斥量，防止A9G数据发送阿里云的通道被占用 */
    rt_mutex_take(A9G_Occupy_Mutex, RT_WAITING_FOREVER);

    rt_device_write(A9G_UART_Serial, 0, "AT+GPS=1\r\n", sizeof("AT+GPS=1\r\n"));
    while(1)
    {
        if(Location_Step_Cnt == 4)
            break;
        if(rt_event_recv(&A9G_EventSet,
                        (OK_FeedBack_Event | Location_Error_Event),
                         RT_EVENT_FLAG_OR,
                         RT_WAITING_FOREVER,
                         &uEventVal_A9G) == RT_EOK)
        {
            if(rt_event_recv(&A9G_EventSet,
                             OK_FeedBack_Event,
                            (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                             RT_WAITING_NO,
                             &uEventVal_A9G) == RT_EOK)
            {
                switch(Location_Step_Cnt)
                {
                case 0:
                    rt_device_write(A9G_UART_Serial, 0, "AT+GPSRD=5\r\n", sizeof("AT+GPSRD=5\r\n"));
                    rt_thread_mdelay(15000);
                    break;
                case 1:
                    rt_device_write(A9G_UART_Serial, 0, "AT+GPSRD=0\r\n", sizeof("AT+GPSRD=0\r\n"));
                    break;
                case 2:
                    rt_device_write(A9G_UART_Serial, 0, "AT+LOCATION=2\r\n", sizeof("AT+LOCATION=2\r\n"));
                    break;
                case 3:
                    /* 定位成功则将定位数据拆分成整数和小数部分分别发送阿里云 */
                    Location_Apart_Int_Frac_Send();

                    /* 释放互斥量，此时别的功能可占用A9G模块 */
                    rt_mutex_release(A9G_Occupy_Mutex);
                    break;
                }
                Location_Step_Cnt ++;
            }
            if(rt_event_recv(&A9G_EventSet,
                             Location_Error_Event,
                            (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                             RT_WAITING_NO,
                             &uEventVal_A9G) == RT_EOK)
            {
                /* 释放互斥量，此时别的功能可占用A9G模块  */
                rt_mutex_release(A9G_Occupy_Mutex);
                break;
            }
        }
    }
    while(1)
    {
        /* 占用互斥量，防止A9G数据发送阿里云的通道被占用 */
        rt_mutex_take(A9G_Occupy_Mutex, RT_WAITING_FOREVER);

        rt_device_write(A9G_UART_Serial, 0, "AT+LOCATION=2\r\n", sizeof("AT+LOCATION=2\r\n"));
        if(rt_event_recv(&A9G_EventSet,
                        (OK_FeedBack_Event | Location_Error_Event),
                         RT_EVENT_FLAG_OR,
                         RT_WAITING_FOREVER,
                         &uEventVal_A9G) == RT_EOK)
        {
            if(rt_event_recv(&A9G_EventSet,
                             OK_FeedBack_Event,
                            (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                             RT_WAITING_NO,
                             &uEventVal_A9G) == RT_EOK)
            {
                /* 定位成功则将定位数据拆分成整数和小数部分分别发送阿里云 */
                Location_Apart_Int_Frac_Send();

                /* 释放互斥量，此时别功能可占用A9G模块  */
                rt_mutex_release(A9G_Occupy_Mutex);
            }
            if(rt_event_recv(&A9G_EventSet,
                             Location_Error_Event,
                            (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                             RT_WAITING_NO,
                             &uEventVal_A9G) == RT_EOK)
            {
                /* 释放互斥量，此时别功能可占用A9G模块  */
                rt_mutex_release(A9G_Occupy_Mutex);
            }
        }
        rt_thread_mdelay(10000);
    }
}


/*
 * A9G线程开启函数
 * */
int A9G_MQTT_GPS_Start(void)
{
    static struct rt_thread A9G_UART_Rx_Thread;//定义A9G接收线程
    static rt_thread_t A9G_Init_Ops_Send_Thread;//定义A9G初始化操作发送线程
    static rt_thread_t Get_Location_Thread;//定义A9G定位操作线程

    ALIGN(RT_ALIGN_SIZE)
    static char A9G_UART_Rx_Thread_Stack[1024];//创建A9G接收线程栈，并且地址4字节对齐（动态创建线程无需这一步骤）

    /* 查找系统中的串口设备 */
    A9G_UART_Serial = rt_device_find(A9G_UART);
    if (!A9G_UART_Serial)
    {
        rt_kprintf("find %s failed!\n", A9G_UART);
        return RT_ERROR;
    }
    Interaction_UART_Serial = rt_device_find(Interaction_UART);
    if (!Interaction_UART_Serial)
    {
        rt_kprintf("find %s failed!\n", Interaction_UART);
        return RT_ERROR;
    }
    /* 初始化A9G接收信号量 */
    rt_sem_init(&A9G_UART_Rx_Sem, "A9G_UART_Rx_Sem_Name", 0, RT_IPC_FLAG_FIFO);

    /* 以中断接收及轮询发送模式打开串口设备 */
    rt_device_open(A9G_UART_Serial, RT_DEVICE_FLAG_INT_RX);
    rt_device_open(Interaction_UART_Serial, RT_DEVICE_FLAG_INT_RX);

    /* 设置A9G串口接收回调函数 */
    rt_device_set_rx_indicate(A9G_UART_Serial, A9G_Rx_CallBack);

    /* 静态创建并开启 A9G串口接收线程 */
    rt_thread_init(&A9G_UART_Rx_Thread,
                   "A9G_UART_Rx_Thread_Name",
                    A9G_UART_Rx_Entry,
                    RT_NULL,
                    &A9G_UART_Rx_Thread_Stack[0],
                    STACK_SIZE_1024,
                    PRIORITY_11, TIMESLICE_5);
    rt_thread_startup(&A9G_UART_Rx_Thread);

    /* 动态创建并开启A9G初始化操作线程 */
    A9G_Init_Ops_Send_Thread = rt_thread_create("A9G_Init_Ops_Send_Thread_Name",
                                                 A9G_Init_Ops_Send_Entry,
                                                 RT_NULL,
                                                 STACK_SIZE_1024,
                                                 PRIORITY_12,
                                                 TIMESLICE_5);
    if(A9G_Init_Ops_Send_Thread != RT_NULL)
        rt_thread_startup(A9G_Init_Ops_Send_Thread);

    /* 动态创建并开启A9G定位操作线程 */
    Get_Location_Thread = rt_thread_create("Get_Location_Thread_Name",
                                            Get_Location_Entry,
                                            RT_NULL,
                                            STACK_SIZE_1024,
                                            PRIORITY_13,
                                            TIMESLICE_5);
    if(Get_Location_Thread != RT_NULL)
        rt_thread_startup(Get_Location_Thread);
    return 0;
}

/*
 * @brief 定位数据类型变换为float类型
 * @param 接收的数据的储存数组
 * @retval 无
 * */
void Location_To_Float(char* p_array)
{
    char aLati_PreCache[32] ;//纬度数据缓存数组
    char aLongi_PreCache[32] ;//经度数据缓存数组
    uint8_t uChar_Bit_Cnt = 0;//字符数组数量计数
    uint8_t uVal_Assign_Cnt = 0;//赋值计数
    uint8_t uSeparator_Bit;//分隔符位置

    /* 分隔符定位及字符数量统计 */
    while(1)
    {
        if(*(p_array + uChar_Bit_Cnt) == 0x2C)//0x2C","
        {
            uSeparator_Bit = uChar_Bit_Cnt;
        }
        if(*(p_array + uChar_Bit_Cnt) == 0x0D)//0x0D"\r"
        {
            uChar_Bit_Cnt -= 1;//计数到最后一个字符，不包括\r
            break;
        }
        uChar_Bit_Cnt ++;
    }

    /* 经纬度赋值 */
    for(uVal_Assign_Cnt = 0; uVal_Assign_Cnt < (uSeparator_Bit - 1); uVal_Assign_Cnt ++)
    {
        /* 纬度缓存数组赋值 */
        aLati_PreCache[uVal_Assign_Cnt] = *(char*)(p_array + uVal_Assign_Cnt + 1);
    }
    uVal_Assign_Cnt = 0;
    for(uVal_Assign_Cnt = uSeparator_Bit + 1;uVal_Assign_Cnt <= uChar_Bit_Cnt;uVal_Assign_Cnt ++)
    {
        /* 经度缓存数组赋值 */
        aLongi_PreCache[uVal_Assign_Cnt - (uSeparator_Bit + 1)] = *(char*)(p_array + uVal_Assign_Cnt);
    }

    /* 经纬度缓存数组转float型数据 */
    fLati_Val = atof((char*)&aLati_PreCache);
    fLongi_Val = atof((char*)&aLongi_PreCache);
}

/*
 * 定位数据转换成整数部分和小数部分后发送
 * */
void Location_Apart_Int_Frac_Send(void)
{
    uint8_t Longi_Val_Int = 0;//经度整数部分
    uint32_t Longi_Val_Frac = 0;//经度小数部分
    uint8_t Lati_Val_Int = 0;//纬度整数部分
    uint32_t Lati_Val_Frac = 0;//纬度小数部分
    double Roll_Val = 0.0;//运算转存部分

    /* 经度整数化处理 */
    Longi_Val_Int = (uint8_t)fLongi_Val;
    Roll_Val = fLongi_Val - Longi_Val_Int;
    Longi_Val_Frac = (uint8_t)(Roll_Val * pow(10 , 6));

    /* 纬度整数化处理 */
    Lati_Val_Int = (uint8_t)fLati_Val;
    Roll_Val = fLati_Val - Lati_Val_Int;
    Lati_Val_Frac = (uint8_t)(Roll_Val * pow(10 , 6));

    /* 定位数据发送阿里云 */
    my_printf("AT+MQTTPUB=\"/sys/in32QSarJUf/Device_01/thing/event/property/post\",\"{\\22id\\22:\\22123\\22,\\22version\\22:\\221.0\\22,\\22params\\22:{\\22Longitude\\22:%d.%d,\\22Latitude\\22:%d.%d},\\22method\\22:\\22thing.event.property.post\\22}\",0,0,0\r\n",
            Longi_Val_Int,
            Longi_Val_Frac,
            Lati_Val_Int,
            Lati_Val_Frac);//上传数据到阿里云
    rt_thread_mdelay(1000);

    /* 发送定位状态询问消息，目的只是为了解决发送数据后反馈消息接收不完全的问题 */
    rt_device_write(A9G_UART_Serial, 0, "AT+GPS=?\r\n", sizeof("AT+GPS=?\r\n"));

    /* 接收多余的OK事件反馈 */
    rt_event_recv(&A9G_EventSet,
                  OK_FeedBack_Event,
                 (RT_EVENT_FLAG_OR | RT_EVENT_FLAG_CLEAR),
                  RT_WAITING_FOREVER,
                  &uEventVal_A9G);
}
