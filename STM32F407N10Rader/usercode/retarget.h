/**
 * @file    retarget.h
 * @brief   printf重定向库 - 支持任意HAL串口
 * @note    使用方法: 
 *          1. 包含此头文件
 *          2. 调用 Retarget_Init(&huartx) 初始化
 *          3. 直接使用 printf() 输出
 */

#ifndef __RETARGET_H__
#define __RETARGET_H__

#ifdef __cplusplus
extern "C" {
#endif

/* 自动检测HAL库类型 */
#if defined(STM32F0)
    #include "stm32f0xx_hal.h"
#elif defined(STM32F1)
    #include "stm32f1xx_hal.h"
#elif defined(STM32F2)
    #include "stm32f2xx_hal.h"
#elif defined(STM32F3)
    #include "stm32f3xx_hal.h"
#elif defined(STM32F4)
    #include "stm32f4xx_hal.h"
#elif defined(STM32F7)
    #include "stm32f7xx_hal.h"
#elif defined(STM32G0)
    #include "stm32g0xx_hal.h"
#elif defined(STM32G4)
    #include "stm32g4xx_hal.h"
#elif defined(STM32H7)
    #include "stm32h7xx_hal.h"
#elif defined(STM32L0)
    #include "stm32l0xx_hal.h"
#elif defined(STM32L1)
    #include "stm32l1xx_hal.h"
#elif defined(STM32L4)
    #include "stm32l4xx_hal.h"
#elif defined(STM32L5)
    #include "stm32l5xx_hal.h"
#elif defined(STM32U5)
    #include "stm32u5xx_hal.h"
#elif defined(STM32WB)
    #include "stm32wbxx_hal.h"
#elif defined(STM32WL)
    #include "stm32wlxx_hal.h"
#else
    /* 如果没有检测到，用户需要手动包含对应的HAL头文件 */
    #include "main.h"
#endif

/**
 * @brief  初始化printf重定向
 * @param  huart: 串口句柄指针 (如 &huart1)
 * @retval None
 * @note   调用此函数后即可使用printf输出到指定串口
 */
void Retarget_Init(UART_HandleTypeDef *huart);

#ifdef __cplusplus
}
#endif

#endif /* __RETARGET_H__ */
