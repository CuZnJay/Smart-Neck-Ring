#ifndef __LED_H
#define __LED_H

#include "ch32v30x.h"
#include <rtthread.h>
#include <rthw.h>
#include "drivers/pin.h"
#include <board.h>
#include "drv_gpio.h"

#define LED0 GET_PIN(A,15)
#define LED1 GET_PIN(B,4)
#define LED  GET_PIN(E,1)

void LED_Twinkle(rt_base_t pin, rt_uint16_t interval, rt_uint8_t count);
#endif
