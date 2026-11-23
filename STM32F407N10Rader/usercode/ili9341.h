/**
 * @file ili9341.h
 * @brief ILI9341 LCD驱动库 (HAL库版本)
 * @version 2.0
 * @date 2024
 * 
 * @details 使用STM32 HAL库实现SPI通信
 *          - 支持阻塞模式和DMA模式
 *          - 支持4个显示方向
 *          - 提供基础绘图函数
 * 文件头注释 (版本、功能说明)
├── 工作模式配置
├── LCD参数配置
├── 硬件引脚配置
├── 引脚控制宏
├── 常用颜色定义 (RGB565)
├── LCD设备结构体
├── 函数声明 - 初始化与配置
├── 函数声明 - 背光控制
├── 函数声明 - 窗口与光标
├── 函数声明 - 底层SPI通信 (阻塞模式)
├── 函数声明 - DMA传输模式
└── 函数声明 - 绘图函数
 */

#ifndef __ILI9341_H__
#define __ILI9341_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "main.h"             
#include "spi.h"              
#include "gpio.h"             
#include "stm32f4xx_hal.h"  

/* ============================================================================
 * 工作模式配置
 * ============================================================================ */

/** @brief DMA模式开关 (1:DMA模式 0:阻塞模式) */
#define LCD_USE_DMA          1

/* ============================================================================
 * LCD参数配置
 * ============================================================================ */

/** @brief LCD物理宽度 (竖屏时) */
#define LCD_WIDTH           240

/** @brief LCD物理高度 (竖屏时) */
#define LCD_HEIGHT          320

/** @brief SPI句柄 */
#define LCD_SPI_HANDLE      hspi2

/** @brief 默认显示方向 (0:竖屏 1:横屏 2:竖屏180° 3:横屏180°) */
#define USE_HORIZONTAL      0

/* ============================================================================
 * 硬件引脚配置 - 根据实际连接修改
 * ============================================================================ */

/* RST复位引脚 */
#define LCD_RST_GPIO_Port   GPIOD
#define LCD_RST_Pin         GPIO_PIN_4

/* DC数据/命令选择引脚 */
#define LCD_DC_GPIO_Port    GPIOD
#define LCD_DC_Pin          GPIO_PIN_5

/* CS片选引脚 */
#define LCD_CS_GPIO_Port    GPIOD
#define LCD_CS_Pin          GPIO_PIN_6

/* LED背光引脚 */
#define LCD_LED_GPIO_Port   GPIOD
#define LCD_LED_Pin         GPIO_PIN_7

/* ============================================================================
 * 引脚控制宏 (HAL库GPIO操作)
 * ============================================================================ */

/** @brief 复位引脚拉低 */
#define LCD_RST_Clr()       HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET)
/** @brief 复位引脚拉高 */
#define LCD_RST_Set()       HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET)

/** @brief DC引脚拉低 (命令模式) */
#define LCD_DC_Clr()        HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET)
/** @brief DC引脚拉高 (数据模式) */
#define LCD_DC_Set()        HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET)

/** @brief 片选引脚拉低 (选中) */
#define LCD_CS_Clr()        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET)
/** @brief 片选引脚拉高 (取消选中) */
#define LCD_CS_Set()        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET)

/** @brief 背光开启 */
#define LCD_LED_ON()        HAL_GPIO_WritePin(LCD_LED_GPIO_Port, LCD_LED_Pin, GPIO_PIN_SET)
/** @brief 背光关闭 */
#define LCD_LED_OFF()       HAL_GPIO_WritePin(LCD_LED_GPIO_Port, LCD_LED_Pin, GPIO_PIN_RESET)

/* ============================================================================
 * 常用颜色定义 (RGB565格式)
 * ============================================================================ */

#define WHITE          0xFFFF   /**< 白色 */
#define BLACK          0x0000   /**< 黑色 */
#define BLUE           0x001F   /**< 蓝色 */
#define BRED           0xF81F   /**< 蓝红色 */
#define GRED           0xFFE0   /**< 绿红色 */
#define GBLUE          0x07FF   /**< 绿蓝色 */
#define RED            0xF800   /**< 红色 */
#define MAGENTA        0xF81F   /**< 品红色 */
#define GREEN          0x07E0   /**< 绿色 */
#define CYAN           0x7FFF   /**< 青色 */
#define YELLOW         0xFFE0   /**< 黄色 */
#define BROWN          0xBC40   /**< 棕色 */
#define BRRED          0xFC07   /**< 棕红色 */
#define GRAY           0x8430   /**< 灰色 */
#define ORANGE         0xFD20   /**< 橙色 */

/* ============================================================================
 * LCD设备结构体
 * ============================================================================ */

/**
 * @brief LCD设备参数结构体
 */
typedef struct {
    uint16_t width;         /**< 当前显示宽度 */
    uint16_t height;        /**< 当前显示高度 */
    uint16_t id;            /**< LCD驱动IC ID */
    uint8_t  dir;           /**< 显示方向 (0-3) */
    uint16_t wramcmd;       /**< 写GRAM命令 (0x2C) */
    uint16_t setxcmd;       /**< 设置X坐标命令 (0x2A) */
    uint16_t setycmd;       /**< 设置Y坐标命令 (0x2B) */
} _lcd_dev;

/** @brief LCD设备实例 (全局) */
extern _lcd_dev lcddev;

/* ============================================================================
 * 函数声明 - 初始化与配置
 * ============================================================================ */

/**
 * @brief LCD初始化
 * @details 执行硬件复位、发送初始化序列、设置默认方向、清屏
 */
void LCD_Init(void);

/**
 * @brief LCD硬件复位
 */
void LCD_Reset(void);

/**
 * @brief 设置显示方向
 * @param[in] direction 方向值
 *            - 0: 竖屏 (240x320)
 *            - 1: 横屏 (320x240)
 *            - 2: 竖屏180° (240x320)
 *            - 3: 横屏180° (320x240)
 */
void LCD_SetDirection(uint8_t direction);

/**
 * @brief 获取当前显示方向
 * @return uint8_t 当前方向 (0-3)
 */
uint8_t GetLcdDir(void);

/* ============================================================================
 * 函数声明 - 背光控制
 * ============================================================================ */

/**
 * @brief 开启背光
 */
void LCD_BackLight_On(void);

/**
 * @brief 关闭背光
 */
void LCD_BackLight_Off(void);

/* ============================================================================
 * 函数声明 - 窗口与光标
 * ============================================================================ */

/**
 * @brief 设置显示窗口
 * @param[in] startX 起始X坐标
 * @param[in] startY 起始Y坐标
 * @param[in] width  窗口宽度
 * @param[in] height 窗口高度
 */
void LCD_SetWindow(uint16_t startX, uint16_t startY, uint16_t width, uint16_t height);

/**
 * @brief 设置光标位置
 * @param[in] x X坐标
 * @param[in] y Y坐标
 */
void LCD_SetCursor(uint16_t x, uint16_t y);

/**
 * @brief 准备写入GRAM
 * @details 发送写GRAM命令 (0x2C)，之后可连续写入像素数据
 */
void LCD_WriteRAM_Prepare(void);

/* ============================================================================
 * 函数声明 - 底层SPI通信 (阻塞模式)
 * ============================================================================ */

/**
 * @brief 发送命令 (阻塞模式)
 * @param[in] cmd 命令字节
 */
void LCD_SendCmd(uint8_t cmd);

/**
 * @brief 发送8位数据 (阻塞模式)
 * @param[in] data 数据字节
 */
void LCD_SendData(uint8_t data);

/**
 * @brief 写寄存器
 * @param[in] reg  寄存器地址
 * @param[in] data 16位数据
 */
void LCD_WriteReg(uint8_t reg, uint16_t data);

/**
 * @brief 发送16位数据 (阻塞模式)
 * @param[in] data 16位数据 (高字节先发)
 */
void LCD_WriteData_16Bit(uint16_t data);

/* ============================================================================
 * 函数声明 - DMA传输模式
 * ============================================================================ */

#if LCD_USE_DMA

/**
 * @brief 发送命令 (DMA模式)
 * @param[in] cmd 命令字节
 */
void LCD_SendCmd_DMA(uint8_t cmd);

/**
 * @brief 发送8位数据 (DMA模式)
 * @param[in] data 数据字节
 */
void LCD_SendData_DMA(uint8_t data);

/**
 * @brief DMA方式写一行像素数据
 * @param[in] line_buffer 像素缓冲区 (需预先交换字节序)
 * @param[in] width       像素数量
 * @note 缓冲区数据需为大端序 (MSB first)
 */
void LCD_WriteLine_DMA(uint16_t* line_buffer, uint32_t width);

/**
 * @brief 启动块DMA传输 (非阻塞)
 * @param[in] buffer     像素缓冲区 (需预先交换字节序)
 * @param[in] pixelCount 像素数量
 */
void LCD_StartBlockDMA(uint16_t* buffer, uint32_t pixelCount);

/**
 * @brief 等待块DMA传输完成
 */
void LCD_WaitBlockDMA(void);

/**
 * @brief 检查块DMA是否正在传输
 * @return uint8_t 1=忙碌 0=空闲
 */
uint8_t LCD_IsBlockDMABusy(void);

/**
 * @brief DMA方式清屏
 * @param[in] color RGB565颜色值
 */
void LCD_Clear_DMA(uint16_t color);

#endif /* LCD_USE_DMA */

/* ============================================================================
 * 函数声明 - 绘图函数
 * ============================================================================ */

/**
 * @brief 清屏 (自动选择DMA或阻塞模式)
 * @param[in] color RGB565颜色值
 */
void LCD_Clear(uint16_t color);

/**
 * @brief 画点
 * @param[in] x     X坐标
 * @param[in] y     Y坐标
 * @param[in] color RGB565颜色值
 */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief 画线 (Bresenham算法)
 * @param[in] x1    起点X坐标
 * @param[in] y1    起点Y坐标
 * @param[in] x2    终点X坐标
 * @param[in] y2    终点Y坐标
 * @param[in] color RGB565颜色值
 */
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color);

/**
 * @brief 画实心圆
 * @param[in] centerX 圆心X坐标
 * @param[in] centerY 圆心Y坐标
 * @param[in] radius  半径
 * @param[in] color   RGB565颜色值
 */
void LCD_DrawFilledCircle(uint16_t centerX, uint16_t centerY, uint16_t radius, uint16_t color);

#ifdef __cplusplus
}
#endif


#endif // __ILI9341_H__


