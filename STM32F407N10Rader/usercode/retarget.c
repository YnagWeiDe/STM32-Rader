/**
 * @file    retarget.c
 * @brief   printf重定向库实现
 */

#include "retarget.h"
#include <stdio.h>

/* 存储串口句柄指针 */
static UART_HandleTypeDef *s_pUartHandle = NULL;

/**
 * @brief  初始化printf重定向
 * @param  huart: 串口句柄指针
 */
void Retarget_Init(UART_HandleTypeDef *huart)
{
    s_pUartHandle = huart;
}

/* ===================== 重定向实现 ===================== */

#if defined(__ARMCC_VERSION) && (__ARMCC_VERSION >= 6010050)
/* ARM Compiler 6 (AC6) */
__asm(".global __use_no_semihosting");

struct __FILE
{
    int handle;
};

FILE __stdout;
FILE __stdin;
FILE __stderr;

int fputc(int ch, FILE *f)
{
    (void)f;
    if (s_pUartHandle != NULL)
    {
        HAL_UART_Transmit(s_pUartHandle, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    }
    return ch;
}

int fgetc(FILE *f)
{
    (void)f;
    uint8_t ch = 0;
    if (s_pUartHandle != NULL)
    {
        HAL_UART_Receive(s_pUartHandle, &ch, 1, HAL_MAX_DELAY);
    }
    return (int)ch;
}

void _sys_exit(int return_code)
{
    (void)return_code;
    while (1);
}

void _ttywrch(int ch)
{
    if (s_pUartHandle != NULL)
    {
        HAL_UART_Transmit(s_pUartHandle, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    }
}

#elif defined(__ARMCC_VERSION)
/* ARM Compiler 5 (AC5) */
#pragma import(__use_no_semihosting)

struct __FILE
{
    int handle;
};

FILE __stdout;
FILE __stdin;
FILE __stderr;

int fputc(int ch, FILE *f)
{
    (void)f;
    if (s_pUartHandle != NULL)
    {
        HAL_UART_Transmit(s_pUartHandle, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    }
    return ch;
}

int fgetc(FILE *f)
{
    (void)f;
    uint8_t ch = 0;
    if (s_pUartHandle != NULL)
    {
        HAL_UART_Receive(s_pUartHandle, &ch, 1, HAL_MAX_DELAY);
    }
    return (int)ch;
}

void _sys_exit(int return_code)
{
    (void)return_code;
    while (1);
}

void _ttywrch(int ch)
{
    if (s_pUartHandle != NULL)
    {
        HAL_UART_Transmit(s_pUartHandle, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    }
}

#elif defined(__GNUC__)
/* GCC */
int __io_putchar(int ch)
{
    if (s_pUartHandle != NULL)
    {
        HAL_UART_Transmit(s_pUartHandle, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    }
    return ch;
}

int __io_getchar(void)
{
    uint8_t ch = 0;
    if (s_pUartHandle != NULL)
    {
        HAL_UART_Receive(s_pUartHandle, &ch, 1, HAL_MAX_DELAY);
    }
    return (int)ch;
}

int _write(int file, char *ptr, int len)
{
    (void)file;
    if (s_pUartHandle != NULL)
    {
        HAL_UART_Transmit(s_pUartHandle, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    }
    return len;
}

int _read(int file, char *ptr, int len)
{
    (void)file;
    if (s_pUartHandle != NULL)
    {
        HAL_UART_Receive(s_pUartHandle, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    }
    return len;
}

#elif defined(__ICCARM__)
/* IAR */
int fputc(int ch, FILE *f)
{
    (void)f;
    if (s_pUartHandle != NULL)
    {
        HAL_UART_Transmit(s_pUartHandle, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    }
    return ch;
}

int fgetc(FILE *f)
{
    (void)f;
    uint8_t ch = 0;
    if (s_pUartHandle != NULL)
    {
        HAL_UART_Receive(s_pUartHandle, &ch, 1, HAL_MAX_DELAY);
    }
    return (int)ch;
}

#endif
