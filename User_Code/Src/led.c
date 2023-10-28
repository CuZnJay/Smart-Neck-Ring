#include <board.h>
#include <drivers/pin.h>
#include "ch32v30x.h"
#include <drivers/pin.h>

#include "led.h"

/**
  * @brief LED闪烁
  * @param pin LED的引脚号
  * @param interval 闪烁间隔
  * @param count 闪烁次数
  */
void LED_Twinkle(rt_base_t pin, rt_uint16_t interval, rt_uint8_t count)
{
    rt_uint8_t uNum;
    for(uNum = 0;uNum < count;uNum++)
    {
        rt_pin_write(pin, 1);
        rt_thread_delay(interval);
        rt_pin_write(pin, 0);
        rt_thread_delay(interval);
    }
}
