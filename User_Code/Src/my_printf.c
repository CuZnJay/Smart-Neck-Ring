#include <board.h>
#include <drivers/pin.h>
#include "ch32v30x.h"
#include <drivers/pin.h>
#include <stdarg.h>
#include <math.h>
#include <stdio.h>

#include "my_printf.h"
#include "A9G.h"
#include "led.h"

/**
 * @brief 自定义printf函数，通过USART2打印
 * @brief 仅添加打印：整数，浮点数，字符，字符串的功能(浮点数功能与系统的浮点数打印功能冲突，尽量不使用，可通过%d.%d打印浮点数)
 * @brief 打印字符需添加"",例如my_printf("prtint a char %c\r\n", "A");
 */
void my_printf(const char* str, ...)
{
    char* pStr = (char*)str;// 指向str
    static char aUART_TxBuffer[512];//发送内容缓存

    rt_uint8_t uData_Len_Cnt = 0;//数据长度计数
    rt_uint8_t uBuffer_Len_Cnt = 0;//缓存数组赋值计数

    char* ArgStrVal = NULL;  // 接收字符型

    rt_uint32_t ArgIntVal = 0;  // 接收整型

    double ArgFloVal = 0.0; // 接收浮点型数据
    rt_uint32_t ArgFloVal_Int = 0;  //储存浮点型数据整数部分
    double ArgFloVal_Frac = 0.0;  //储存浮点型数据小数部分
    rt_uint32_t ArgFloVal_Frac_32t = 0;//储存浮点型数据小数部分在小数点向右移动后的整数部分

    rt_uint32_t val_seg = 0;   // 数据切分暂存值

#ifdef MY_PRINTF_USING_USART2
    if (str == NULL)
        NULL;
    va_list pArgs; // 定义va_list类型指针，用于存储参数的地址
    va_start(pArgs, str); // 初始化pArgs

    /*开始不断检索要打印的字符,直到检索到'\0'停止，并将检索到的字符加入要发送的字符数组中*/
    while(*pStr != '\0')
    {
        /*检索到格式化字符‘%’后根据转义字符进行相应操作*/
        switch(*pStr)
        {
        /*检索到格式化字符*/
        case '%':
            pStr++;
            switch(*pStr)
            {
            case ' ':
                aUART_TxBuffer[uBuffer_Len_Cnt] = ' ';
                uBuffer_Len_Cnt++;
                pStr++;
                continue;
            case '%':
                aUART_TxBuffer[uBuffer_Len_Cnt] = '%';
                uBuffer_Len_Cnt++;
                pStr++;
                continue;
                /*打印字符*/
            case 'c':
                ArgStrVal = va_arg(pArgs, char*);
                aUART_TxBuffer[uBuffer_Len_Cnt] = *(char*)ArgStrVal;
                uBuffer_Len_Cnt++;
                pStr++;
                continue;
                /*打印字符串*/
            case 's':
                ArgStrVal = va_arg(pArgs, char*);
                while(*ArgStrVal != '\0')
                {
                    aUART_TxBuffer[uBuffer_Len_Cnt] = *ArgStrVal;
                    ArgStrVal += 1;
                    uBuffer_Len_Cnt++;
                }
                pStr++;
                continue;
                /*打印整数*/
            case 'd':
                ArgIntVal = va_arg(pArgs, int);//返回当前参数值，并指向下一个参数，pArgs为当前参数地址，int为当前参数类型
                /*判断是否为0*/
                if(ArgIntVal == 0)
                {
                    aUART_TxBuffer[uBuffer_Len_Cnt] = '0';//打印0
                    uBuffer_Len_Cnt++;
                }
                /*判断正负*/
                if(ArgIntVal < 0)
                {
                    ArgIntVal = -ArgIntVal;
                    aUART_TxBuffer[uBuffer_Len_Cnt] = '-';//添加负号
                    uBuffer_Len_Cnt++;
                }
                else
                {
                    val_seg = ArgIntVal;
                    while(val_seg)
                    {
                        val_seg /= 10;
                        uData_Len_Cnt++;
                    }
                    while(uData_Len_Cnt > 0)
                    {
                        val_seg = ArgIntVal / (rt_uint32_t)pow(10, uData_Len_Cnt - 1);
                        ArgIntVal %= (rt_uint32_t)pow(10, uData_Len_Cnt - 1);
                        aUART_TxBuffer[uBuffer_Len_Cnt] = (char)('0' + (char)val_seg);
                        uBuffer_Len_Cnt++;
                        uData_Len_Cnt--;
                    }
                }
                pStr++;
                continue;
                /*打印浮点数*/
            case 'f':
                ArgFloVal = va_arg(pArgs, double);//返回当前参数值，并指向下一个参数，pArgs为当前参数地址，double为当前参数类型
                /*判断正负*/
                if(ArgFloVal < 0)
                {
                    ArgFloVal = -ArgFloVal;
                    aUART_TxBuffer[uBuffer_Len_Cnt] = '-';//添加负号
                    uBuffer_Len_Cnt++;
                }
                ArgFloVal_Int = (rt_uint32_t)ArgFloVal;//取整数部分
                ArgFloVal_Frac = ArgFloVal - ArgFloVal_Int;

                val_seg = ArgFloVal_Int;
                if(ArgFloVal_Int == 0)
                {
                    aUART_TxBuffer[uBuffer_Len_Cnt] = '0';//打印0
                    uBuffer_Len_Cnt++;
                }
                while(val_seg)
                {
                    val_seg /= 10;
                    uData_Len_Cnt++;
                }
                while(uData_Len_Cnt > 0)
                {
                    val_seg = ArgFloVal_Int / (rt_uint32_t)pow(10, uData_Len_Cnt - 1);
                    ArgFloVal_Int %= (rt_uint32_t)pow(10, uData_Len_Cnt - 1);
                    aUART_TxBuffer[uBuffer_Len_Cnt] = (char)('0' + (char)val_seg);
                    uBuffer_Len_Cnt++;
                    uData_Len_Cnt--;
                }
                aUART_TxBuffer[uBuffer_Len_Cnt] = '.';//打印小数点
                uBuffer_Len_Cnt++;


                ArgFloVal_Frac *= pow(10, 6);//保留6位小数
                ArgFloVal_Frac_32t = (rt_uint32_t)ArgFloVal_Frac;//取整数部分
                val_seg = ArgFloVal_Frac_32t;
                if(ArgFloVal_Frac_32t == 0)
                {
                    aUART_TxBuffer[uBuffer_Len_Cnt] = '0';//打印0
                    uBuffer_Len_Cnt++;
                }
                while(val_seg)
                {
                    val_seg /= 10;
                    uData_Len_Cnt++;
                }
                while(uData_Len_Cnt > 0)
                {
                    val_seg = ArgFloVal_Frac_32t / (rt_uint32_t)pow(10, uData_Len_Cnt - 1);
                    ArgFloVal_Frac_32t %= (rt_uint32_t)pow(10, uData_Len_Cnt - 1);
                    aUART_TxBuffer[uBuffer_Len_Cnt] = (char)('0' + (char)val_seg);
                    uBuffer_Len_Cnt++;
                    uData_Len_Cnt--;
                }
                pStr++;
                continue;

            }
        /*未检索到格式化字符*/
        default:
            aUART_TxBuffer[uBuffer_Len_Cnt] = *pStr;
            uBuffer_Len_Cnt++;
            pStr++;
            break;
        }
    }
    rt_device_write(A9G_UART_Serial, 0, &aUART_TxBuffer, uBuffer_Len_Cnt);//发送数据
    va_end(pArgs);// 结束取参数
#endif
}


