/**
 * @file ili9341.c
 * @brief ILI9341 LCD驱动库实现 (HAL库版本)
 * @version 2.0
 * @date 2024
 * 
 * @details 使用STM32 HAL库实现SPI通信
 *          - 支持阻塞模式和DMA模式切换
 *          - DMA模式带超时保护，避免死锁
 *          - 使用静态缓冲区，避免动态内存分配
 * 文件头注释 (版本、功能说明)
├── 第1部分: 阻塞传输模式函数
├── 第2部分: DMA传输模式函数
├── 第3部分: 通用控制函数
├── 第4部分: 绘图函数
├── 第5部分: 方向设置
└── 第6部分: LCD初始化
 */ 

#include "ili9341.h"

/* ============================================================================
 * 全局变量
 * ============================================================================ */

/** @brief LCD设备参数实例 */
_lcd_dev lcddev;

/** @brief SPI句柄外部声明 */
extern SPI_HandleTypeDef LCD_SPI_HANDLE;

/* ============================================================================
 * 第1部分: 阻塞传输模式函数
 * ============================================================================ */

/**
 * @brief 发送命令 (阻塞模式)
 * @param[in] cmd 命令字节
 * @details DC引脚拉低表示命令模式
 */
void LCD_SendCmd(uint8_t cmd)
{
    LCD_DC_Clr();
    LCD_CS_Clr();
    HAL_SPI_Transmit(&LCD_SPI_HANDLE, &cmd, 1, HAL_MAX_DELAY);
    LCD_CS_Set();
}

/**
 * @brief 发送8位数据 (阻塞模式)
 * @param[in] data 数据字节
 * @details DC引脚拉高表示数据模式
 */
void LCD_SendData(uint8_t data)
{
    LCD_DC_Set();
    LCD_CS_Clr();
    HAL_SPI_Transmit(&LCD_SPI_HANDLE, &data, 1, HAL_MAX_DELAY);
    LCD_CS_Set();
}

/**
 * @brief 写寄存器
 * @param[in] reg  寄存器地址 (命令)
 * @param[in] data 16位数据 (高字节先发)
 */
void LCD_WriteReg(uint8_t reg, uint16_t data)
{
    LCD_SendCmd(reg);
    LCD_SendData(data >> 8);
    LCD_SendData(data & 0xFF);
}

/**
 * @brief 准备写入GRAM
 * @details 发送写GRAM命令 (0x2C)
 */
void LCD_WriteRAM_Prepare(void)
{
    LCD_SendCmd(lcddev.wramcmd);
}

/**
 * @brief 发送16位数据 (阻塞模式)
 * @param[in] data 16位数据 (高字节先发)
 */
void LCD_WriteData_16Bit(uint16_t data)
{
    uint8_t tx_buffer[2];
    tx_buffer[0] = data >> 8;
    tx_buffer[1] = data & 0xFF;
    
    LCD_DC_Set();
    LCD_CS_Clr();
    HAL_SPI_Transmit(&LCD_SPI_HANDLE, tx_buffer, 2, HAL_MAX_DELAY);
    LCD_CS_Set();
}

/* ============================================================================
 * 第2部分: DMA传输模式函数
 * ============================================================================ */

#if LCD_USE_DMA

/** @brief DMA传输完成标志 */
volatile uint8_t SPI_Tx_Complete = 1;

/** @brief 清屏用静态缓冲区 (避免malloc) */
static uint16_t clear_line_buffer[320];

/**
 * @brief SPI DMA传输完成回调
 * @param[in] hspi SPI句柄指针
 * @details 由HAL库在DMA传输完成中断中调用
 */
void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
{
    if (hspi == &LCD_SPI_HANDLE) {
        SPI_Tx_Complete = 1;
    }
}

/**
 * @brief 发送命令 (DMA模式)
 * @param[in] cmd 命令字节
 */
void LCD_SendCmd_DMA(uint8_t cmd)
{
    LCD_DC_Clr();
    LCD_CS_Clr();
    HAL_SPI_Transmit_DMA(&LCD_SPI_HANDLE, &cmd, 1);
    while (HAL_SPI_GetState(&LCD_SPI_HANDLE) != HAL_SPI_STATE_READY);
    LCD_CS_Set();
}

/**
 * @brief 发送8位数据 (DMA模式)
 * @param[in] data 数据字节
 */
void LCD_SendData_DMA(uint8_t data)
{
    LCD_DC_Set();
    LCD_CS_Clr();
    HAL_SPI_Transmit_DMA(&LCD_SPI_HANDLE, &data, 1);
    while (HAL_SPI_GetState(&LCD_SPI_HANDLE) != HAL_SPI_STATE_READY);
    LCD_CS_Set();
}

/**
 * @brief DMA方式写一行像素数据
 * @param[in] line_buffer 像素缓冲区 (需预先交换字节序)
 * @param[in] width       像素数量
 * @details 带超时保护，防止DMA传输卡死
 */
void LCD_WriteLine_DMA(uint16_t* line_buffer, uint32_t width)
{
    SPI_Tx_Complete = 0;
    LCD_DC_Set();
    LCD_CS_Clr();
    
    HAL_SPI_Transmit_DMA(&LCD_SPI_HANDLE, (uint8_t*)line_buffer, width * 2);
    
    /* 等待传输完成 (带超时保护) */
    uint32_t timeout = 0;
    while (!SPI_Tx_Complete) {
        if (++timeout > 100000) {
            /* 超时: 强制中止DMA并退出 */
            HAL_SPI_Abort(&LCD_SPI_HANDLE);
            SPI_Tx_Complete = 1;
            break;
        }
    }
    
    LCD_CS_Set();
}

/**
 * @brief 启动块DMA传输 (非阻塞)
 * @param[in] buffer     像素缓冲区 (需预先交换字节序)
 * @param[in] pixelCount 像素数量
 * @details 启动后立即返回，不等待传输完成
 *          调用者需要通过LCD_IsBlockDMABusy()检查传输状态
 */
void LCD_StartBlockDMA(uint16_t* buffer, uint32_t pixelCount)
{
    SPI_Tx_Complete = 0;
    LCD_DC_Set();
    LCD_CS_Clr();
    
    HAL_SPI_Transmit_DMA(&LCD_SPI_HANDLE, (uint8_t*)buffer, pixelCount * 2);
    /* 不等待，立即返回 */
}

/**
 * @brief 等待块DMA传输完成
 * @details 带超时保护，防止死锁
 */
void LCD_WaitBlockDMA(void)
{
    uint32_t timeout = 0;
    while (!SPI_Tx_Complete) {
        if (++timeout > 500000) {  // 更长的超时用于大块传输
            HAL_SPI_Abort(&LCD_SPI_HANDLE);
            SPI_Tx_Complete = 1;
            break;
        }
    }
    LCD_CS_Set();
}

/**
 * @brief 检查块DMA是否正在传输
 * @return uint8_t 1=忙碌 0=空闲
 */
uint8_t LCD_IsBlockDMABusy(void)
{
    return !SPI_Tx_Complete;
}

/**
 * @brief DMA方式清屏
 * @param[in] color RGB565颜色值
 * @details 使用静态缓冲区逐行传输，避免动态内存分配
 */
void LCD_Clear_DMA(uint16_t color)
{
    uint16_t color_swapped = (color >> 8) | (color << 8);
    uint16_t width = lcddev.width;
    uint16_t height = lcddev.height;
    
    /* 填充颜色缓冲区 */
    for (uint16_t i = 0; i < width; i++) {
        clear_line_buffer[i] = color_swapped;
    }
    
    /* 设置全屏窗口 */
    LCD_SetWindow(0, 0, width, height);
    LCD_WriteRAM_Prepare();
    
    /* 逐行DMA传输 */
    for (uint16_t y = 0; y < height; y++) {
        LCD_WriteLine_DMA(clear_line_buffer, width);
    }
}

#endif /* LCD_USE_DMA */

/* ============================================================================
 * 第3部分: 通用控制函数
 * ============================================================================ */

/**
 * @brief LCD硬件复位
 * @details RST引脚拉低50ms后拉高
 */
void LCD_Reset(void)
{
    LCD_RST_Clr();
    HAL_Delay(50);
    LCD_RST_Set();
    HAL_Delay(50);
}

/**
 * @brief 开启背光
 */
void LCD_BackLight_On(void)
{
    LCD_LED_ON();
}

/**
 * @brief 关闭背光
 */
void LCD_BackLight_Off(void)
{
    LCD_LED_OFF();
}

/**
 * @brief 设置显示窗口
 * @param[in] startX 起始X坐标
 * @param[in] startY 起始Y坐标
 * @param[in] width  窗口宽度
 * @param[in] height 窗口高度
 * @details 设置后可连续写入像素数据
 */
void LCD_SetWindow(uint16_t startX, uint16_t startY, uint16_t width, uint16_t height)
{
    uint16_t endX = startX + width - 1;
    uint16_t endY = startY + height - 1;
    
    /* 设置列地址范围 (0x2A) */
    LCD_SendCmd(0x2A);
    LCD_SendData(startX >> 8);
    LCD_SendData(startX & 0xFF);
    LCD_SendData(endX >> 8);
    LCD_SendData(endX & 0xFF);

    /* 设置行地址范围 (0x2B) */
    LCD_SendCmd(0x2B);
    LCD_SendData(startY >> 8);
    LCD_SendData(startY & 0xFF);
    LCD_SendData(endY >> 8);
    LCD_SendData(endY & 0xFF);
    
    /* 准备写入GRAM */
    LCD_WriteRAM_Prepare();
}

/**
 * @brief 设置光标位置
 * @param[in] x X坐标
 * @param[in] y Y坐标
 */
void LCD_SetCursor(uint16_t x, uint16_t y)
{
    LCD_SetWindow(x, y, 1, 1);
}

/**
 * @brief 清屏 (自动选择模式)
 * @param[in] color RGB565颜色值
 * @details 根据LCD_USE_DMA宏自动选择DMA或阻塞模式
 */
void LCD_Clear(uint16_t color)
{
#if LCD_USE_DMA
    LCD_Clear_DMA(color);
#else
    /* 阻塞模式清屏 */
    LCD_SetWindow(0, 0, lcddev.width, lcddev.height);
    
    uint32_t total_pixels = lcddev.width * lcddev.height;
    uint16_t color_swapped = (color >> 8) | (color << 8);
    
    for (uint32_t i = 0; i < total_pixels; i++) {
        LCD_WriteData_16Bit(color_swapped);
    }
#endif
}

/* ============================================================================
 * 第4部分: 绘图函数
 * ============================================================================ */

/**
 * @brief 画点
 * @param[in] x     X坐标
 * @param[in] y     Y坐标
 * @param[in] color RGB565颜色值
 */
void LCD_DrawPoint(uint16_t x, uint16_t y, uint16_t color)
{
    LCD_SetWindow(x, y, 1, 1);
    LCD_WriteData_16Bit(color);
}

/**
 * @brief 画线 (Bresenham算法)
 * @param[in] x1    起点X坐标
 * @param[in] y1    起点Y坐标
 * @param[in] x2    终点X坐标
 * @param[in] y2    终点Y坐标
 * @param[in] color RGB565颜色值
 */
void LCD_DrawLine(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2, uint16_t color)
{
    uint16_t t; 
    int xerr = 0, yerr = 0, delta_x, delta_y, distance; 
    int incx, incy, uRow, uCol; 

    delta_x = x2 - x1;
    delta_y = y2 - y1; 
    uRow = x1; 
    uCol = y1; 
    
    /* 设置X方向步进 */
    if (delta_x > 0) incx = 1;
    else if (delta_x == 0) incx = 0;  /* 垂直线 */
    else { incx = -1; delta_x = -delta_x; }
    
    /* 设置Y方向步进 */
    if (delta_y > 0) incy = 1;
    else if (delta_y == 0) incy = 0;  /* 水平线 */
    else { incy = -1; delta_y = -delta_y; }
    
    /* 选取基本增量坐标轴 */
    distance = (delta_x > delta_y) ? delta_x : delta_y;
    
    /* 画线输出 */
    for (t = 0; t <= distance + 1; t++) {
        LCD_DrawPoint(uRow, uCol, color);
        xerr += delta_x;
        yerr += delta_y;
        if (xerr > distance) {
            xerr -= distance;
            uRow += incx;
        }
        if (yerr > distance) {
            yerr -= distance;
            uCol += incy;
        }
    }
}

/**
 * @brief 画实心圆 (中点圆算法)
 * @param[in] centerX 圆心X坐标
 * @param[in] centerY 圆心Y坐标
 * @param[in] radius  半径
 * @param[in] color   RGB565颜色值
 */
void LCD_DrawFilledCircle(uint16_t centerX, uint16_t centerY, uint16_t radius, uint16_t color)
{
    int16_t x = 0;
    int16_t y = radius;
    int16_t d = 1 - radius;
    int16_t deltaE = 3;
    int16_t deltaSE = -2 * radius + 5;

    /* 画初始水平线 */
    LCD_DrawLine(centerX - radius, centerY, centerX + radius, centerY, color);

    while (y > x) {
        if (d < 0) {
            /* 选择东点 */
            d += deltaE;
            deltaE += 2;
            deltaSE += 2;
        } else {
            /* 选择东南点 */
            d += deltaSE;
            deltaE += 2;
            deltaSE += 4;
            y--;
        }
        x++;

        /* 画对称的四条水平线 */
        LCD_DrawLine(centerX - x, centerY + y, centerX + x, centerY + y, color);
        LCD_DrawLine(centerX - x, centerY - y, centerX + x, centerY - y, color);
        LCD_DrawLine(centerX - y, centerY + x, centerX + y, centerY + x, color);
        LCD_DrawLine(centerX - y, centerY - x, centerX + y, centerY - x, color);
    }
}

/* ============================================================================
 * 第5部分: 方向设置
 * ============================================================================ */

/**
 * @brief 设置显示方向
 * @param[in] direction 方向值 (0-3)
 *            - 0: 竖屏 (240x320)
 *            - 1: 横屏 (320x240)
 *            - 2: 竖屏180° (240x320)
 *            - 3: 横屏180° (320x240)
 * @details 通过Memory Access Control (0x36)寄存器设置
 */
void LCD_SetDirection(uint8_t direction)
{
    /* 等待SPI空闲，避免与DMA冲突 */
    while (HAL_SPI_GetState(&LCD_SPI_HANDLE) != HAL_SPI_STATE_READY);
    
    lcddev.setxcmd = 0x2A;
    lcddev.setycmd = 0x2B;
    lcddev.wramcmd = 0x2C;
    lcddev.dir = direction;
    
    switch (direction) {
        case 0:  /* 竖屏 0° */
            lcddev.width = LCD_WIDTH;
            lcddev.height = LCD_HEIGHT;
            LCD_SendCmd(0x36);
            LCD_SendData((1<<3)|(0<<6)|(0<<7));  /* BGR=1 */
            break;
            
        case 1:  /* 横屏 90° */
            lcddev.width = LCD_HEIGHT;
            lcddev.height = LCD_WIDTH;
            LCD_SendCmd(0x36);
            LCD_SendData((1<<3)|(0<<7)|(1<<6)|(1<<5));  /* BGR=1, MV=1, MX=1 */
            break;
            
        case 2:  /* 竖屏 180° */
            lcddev.width = LCD_WIDTH;
            lcddev.height = LCD_HEIGHT;
            LCD_SendCmd(0x36);
            LCD_SendData((1<<3)|(1<<6)|(1<<7));  /* BGR=1, MX=1, MY=1 */
            break;
            
        case 3:  /* 横屏 270° */
            lcddev.width = LCD_HEIGHT;
            lcddev.height = LCD_WIDTH;
            LCD_SendCmd(0x36);
            LCD_SendData((1<<3)|(1<<7)|(1<<5));  /* BGR=1, MY=1, MV=1 */
            break;
            
        default:
            break;
    }
}

/**
 * @brief 获取当前显示方向
 * @return uint8_t 当前方向 (0-3)
 */
uint8_t GetLcdDir(void)
{
    return lcddev.dir;
}

/* ============================================================================
 * 第6部分: LCD初始化
 * ============================================================================ */

/**
 * @brief LCD初始化
 * @details 执行以下步骤:
 *          1. 硬件复位
 *          2. 发送ILI9341初始化序列
 *          3. 设置默认显示方向
 *          4. 开启背光
 *          5. 清屏
 */
void LCD_Init(void)
{
    /* 硬件复位 */
    LCD_Reset();
    HAL_Delay(100);
 
    /* ========== ILI9341 初始化序列 ========== */
    
    /* Power Control A */
    LCD_SendCmd(0xCF);
    LCD_SendData(0x00);
    LCD_SendData(0xD9);
    LCD_SendData(0x30);
    
    /* Power Control B */
    LCD_SendCmd(0xED);
    LCD_SendData(0x64);
    LCD_SendData(0x03);
    LCD_SendData(0x12);
    LCD_SendData(0x81);
    
    /* Driver Timing Control A */
    LCD_SendCmd(0xE8);
    LCD_SendData(0x85);
    LCD_SendData(0x10);
    LCD_SendData(0x7A);
    
    /* Power On Sequence Control */
    LCD_SendCmd(0xCB);
    LCD_SendData(0x39);
    LCD_SendData(0x2C);
    LCD_SendData(0x00);
    LCD_SendData(0x34);
    LCD_SendData(0x02);
    
    /* Pump Ratio Control */
    LCD_SendCmd(0xF7);
    LCD_SendData(0x20);
    
    /* Driver Timing Control B */
    LCD_SendCmd(0xEA);
    LCD_SendData(0x00);
    LCD_SendData(0x00);
    
    /* Power Control 1 - GVDD电压 */
    LCD_SendCmd(0xC0);
    LCD_SendData(0x1B);
    
    /* Power Control 2 - 升压电路 */
    LCD_SendCmd(0xC1);
    LCD_SendData(0x12);
    
    /* VCOM Control 1 */
    LCD_SendCmd(0xC5);
    LCD_SendData(0x08);
    LCD_SendData(0x26);
    
    /* VCOM Control 2 */
    LCD_SendCmd(0xC7);
    LCD_SendData(0xB7);
    
    /* Memory Access Control - 默认方向 */
    LCD_SendCmd(0x36);
    LCD_SendData(0x08);
    
    /* Pixel Format - 16bit/pixel */
    LCD_SendCmd(0x3A);
    LCD_SendData(0x55);
    
    /* Frame Rate Control */
    LCD_SendCmd(0xB1);
    LCD_SendData(0x00);
    LCD_SendData(0x1A);
    
    /* Display Function Control */
    LCD_SendCmd(0xB6);
    LCD_SendData(0x0A);
    LCD_SendData(0xA2);
    
    /* 3Gamma Function Disable */
    LCD_SendCmd(0xF2);
    LCD_SendData(0x00);
    
    /* Gamma Curve Selected */
    LCD_SendCmd(0x26);
    LCD_SendData(0x01);
    
    /* Positive Gamma Correction */
    LCD_SendCmd(0xE0);
    LCD_SendData(0x0F);
    LCD_SendData(0x1D);
    LCD_SendData(0x1A);
    LCD_SendData(0x0A);
    LCD_SendData(0x0D);
    LCD_SendData(0x07);
    LCD_SendData(0x49);
    LCD_SendData(0x66);
    LCD_SendData(0x3B);
    LCD_SendData(0x07);
    LCD_SendData(0x11);
    LCD_SendData(0x01);
    LCD_SendData(0x09);
    LCD_SendData(0x05);
    LCD_SendData(0x04);
    
    /* Negative Gamma Correction */
    LCD_SendCmd(0xE1);
    LCD_SendData(0x00);
    LCD_SendData(0x18);
    LCD_SendData(0x1D);
    LCD_SendData(0x02);
    LCD_SendData(0x0F);
    LCD_SendData(0x04);
    LCD_SendData(0x36);
    LCD_SendData(0x13);
    LCD_SendData(0x4C);
    LCD_SendData(0x07);
    LCD_SendData(0x13);
    LCD_SendData(0x0F);
    LCD_SendData(0x2E);
    LCD_SendData(0x2F);
    LCD_SendData(0x05);
    
    /* Column Address Set (0x2A) */
    LCD_SendCmd(0x2A);
    LCD_SendData(0x00);
    LCD_SendData(0x00);
    LCD_SendData(0x00);
    LCD_SendData(0xEF);
    
    /* Page Address Set (0x2B) */
    LCD_SendCmd(0x2B);
    LCD_SendData(0x00);
    LCD_SendData(0x00);
    LCD_SendData(0x01);
    LCD_SendData(0x3F);
    
    /* Sleep Out (0x11) */
    LCD_SendCmd(0x11);
    HAL_Delay(120);
    
    /* Display On (0x29) */
    LCD_SendCmd(0x29);
    
    /* ========== 初始化完成 ========== */
    
    /* 设置默认显示方向 */
    LCD_SetDirection(USE_HORIZONTAL);
    
    /* 开启背光 */
    LCD_BackLight_On();
    
    /* 清屏为白色 */
    LCD_Clear(WHITE);
}
