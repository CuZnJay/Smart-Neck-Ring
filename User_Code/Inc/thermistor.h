#ifndef __THERMISTOR_H
#define __THERMISTOR_H

#include "ch32v30x.h"
#include <rtthread.h>
#include <rtdbg.h>
#include <board.h>
#include <rtdevice.h>

#define Thermistor_ADC "adc1"

#define Thermistor_ADC_Channel     1           /* ADC 通道 */

extern double Temp_Value;

void Get_Temperature_Start(void);
#endif
