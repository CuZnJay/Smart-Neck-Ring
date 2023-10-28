#ifndef __BEFORE_INIT_H
#define __BEFORE_INIT_H

#define A9G_Init_Refresh_OK_Event       (1<<0)
#define A9G_Init_Refresh_Error_Event    (1<<1)
#define Location_Refresh_OK_Event       (1<<2)
#define Location_Refresh_Error_Event    (1<<3)
#define Temperature_Refresh_Event       (1<<4)
#define Movement_Refresh_Event          (1<<5)

#define Battery_Refresh_Event           (1<<6)
#define Signal_Refresh_OK_Event         (1<<7)
#define Signal_Refresh_Error_Event      (1<<8)

#define A9G_Init_Show_Event             (1<<9)
#define Temperature_Show_Event          (1<<10)
#define Movement_Show_Event             (1<<11)
#define Location_Show_Event             (1<<12)
#define A9G_Init_End_Show_Event         (1<<13)
#define Temperature_End_Show_Event      (1<<14)
#define Movement_End_Show_Event         (1<<15)
#define Location_End_Show_Event         (1<<16)


extern rt_uint32_t uEventVal_A9G;//事件集的数值

/* 各传感器数据的OLED显示刷新事件集 */
extern struct rt_event OLED_Data_Refresh_EventSet;

/* 初始化A9G反馈事件集 */
extern struct rt_event A9G_EventSet;

extern rt_mutex_t A9G_Occupy_Mutex;


void Before_Init(void);
#endif
