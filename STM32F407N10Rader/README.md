# STM32F407 N10雷达项目

**作者: Yang 2026.1.23**

## 项目概述

本项目实现了基于STM32F407的N10激光雷达数据解析和LCD显示功能，包括串口数据解析、DMA高效接收、环形队列缓冲、360度雷达可视化显示。

## 模块架构

```
main.c
├── N10Rader.h/c      # N10雷达数据解析库
├── radar_display.h/c # 雷达LCD显示模块
└── ili9341.h/c       # LCD驱动库
```

## 模块说明

### 1. N10雷达解析库 (N10Rader.h/c)

**功能**: 解析N10激光雷达串口数据

**数据结构**:
```c
typedef struct {
    uint16_t angle;      // 角度 (实际角度*100)
    uint16_t distance;   // 距离 (mm)
    uint8_t  intensity;  // 强度
} N10_Point_t;

typedef struct {
    uint16_t speed_us;                      // 转速信息 (μs)
    uint16_t start_angle;                   // 起始角度 (*100)
    uint16_t stop_angle;                    // 结束角度 (*100)
    N10_Point_t points[N10_POINTS_PER_FRAME]; // 16个点
    uint8_t  valid;                         // 帧有效标志
} N10_Frame_t;

typedef struct {
    uint8_t  rx_buffer[N10_RX_BUFFER_SIZE]; // 接收缓冲区
    uint16_t rx_index;                      // 当前接收位置
    
    /* 帧缓冲队列 (环形队列) */
    N10_Frame_t frame_queue[N10_FRAME_QUEUE_SIZE];
    volatile uint8_t queue_head;            // 写入位置 (中断写)
    volatile uint8_t queue_tail;            // 读取位置 (主循环读)
    volatile uint8_t queue_count;           // 队列中帧数量
    volatile uint32_t lost_frames;          // 丢失帧计数
} N10_Parser_t;
```

**API接口**:
```c
void N10_Parser_Init(N10_Parser_t *parser);
void N10_Parser_RxByte(N10_Parser_t *parser, uint8_t byte);
bool N10_Parser_IsFrameReady(N10_Parser_t *parser);
bool N10_Parser_GetFrame(N10_Parser_t *parser, N10_Frame_t *frame);
uint8_t N10_Parser_GetQueueCount(N10_Parser_t *parser);
uint32_t N10_Parser_GetLostFrames(N10_Parser_t *parser);
void N10_Parser_ProcessDMA(N10_Parser_t *parser, const uint8_t *data, uint16_t len);
float N10_AngleToFloat(uint16_t angle_raw);
float N10_GetRotationHz(uint16_t speed_us);
```

**使用示例**:
```c
// 初始化
N10_Parser_t n10_parser;
N10_Parser_Init(&n10_parser);

// 在串口中断中接收数据
void USART2_IRQHandler(void) {
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        uint8_t byte = (uint8_t)(huart2.Instance->DR & (uint8_t)0xFF);
        N10_Parser_RxByte(&n10_parser, byte);
    }
}

// 在主循环中处理帧
N10_Frame_t frame;
while (N10_Parser_GetFrame(&n10_parser, &frame)) {
    // 处理frame数据
}
```

### 2. 雷达显示模块 (radar_display.h/c)

**功能**: 360度雷达可视化显示，支持LCD方向自适应

**配置参数**:
```c
#define RADAR_CENTER_X      120     // 圆心X
#define RADAR_CENTER_Y      120     // 圆心Y
#define RADAR_RADIUS        100     // 显示半径(像素)
#define RADAR_MAX_DISTANCE  3000    // 最大显示距离(mm)
#define DIST_THRESHOLD_NEAR   100  // 近距离阈值(mm) - 红色
#define DIST_THRESHOLD_MID    300  // 中距离阈值(mm) - 黄色
#define RADAR_ANGLE_STEP      12   // 角度步进(度)
```

**数据结构**:
```c
typedef struct {
    uint16_t min_distance;    // 该扇区最小距离(mm)
    uint8_t  has_data;        // 是否有数据
} RadarSector_t;

typedef struct {
    uint16_t center_x;        // 圆心X
    uint16_t center_y;        // 圆心Y
    uint16_t radius;          // 显示半径
    uint16_t max_distance;    // 最大距离映射
    uint16_t angle_step;      // 角度步进
    uint16_t near_threshold;  // 近距离阈值
    uint16_t mid_threshold;   // 中距离阈值
    RadarSector_t sectors[RADAR_SECTOR_COUNT];  // 扇区数据
} RadarDisplay_t;
```

**API接口**:
```c
void Radar_Init(RadarDisplay_t *radar);
void Radar_ClearData(RadarDisplay_t *radar);
void Radar_UpdatePoint(RadarDisplay_t *radar, uint16_t angle_deg, uint16_t distance);
void Radar_Draw(RadarDisplay_t *radar);
void Radar_DrawBackground(RadarDisplay_t *radar);
uint16_t Radar_GetColorByDistance(uint16_t distance, uint16_t near_threshold, uint16_t mid_threshold);
```

**使用示例**:
```c
// 初始化
RadarDisplay_t radar;
Radar_Init(&radar);
Radar_DrawBackground(&radar);  // 只画一次背景

// 更新数据
Radar_UpdatePoint(&radar, 45, 150);  // 45度方向150mm距离

// 绘制
Radar_Draw(&radar);
Radar_ClearData(&radar);  // 准备下一圈
```

**颜色规则**:
- 🔴 **红色**: ≤100mm (危险)
- 🟡 **黄色**: 100-300mm (警告)
- 🟢 **绿色**: >300mm (安全)

## 完整使用流程

### 1. 硬件连接
- USART2: N10雷达 (230400 bps)
- SPI2: ILI9341 LCD
- DMA: USART2接收

### 2. 初始化代码
```c
#include "N10Rader.h"
#include "radar_display.h"

N10_Parser_t n10_parser;
N10_Frame_t frame;
RadarDisplay_t radar;

// 初始化
LCD_Init();
LCD_SetDirection(1);  // 横屏

Radar_Init(&radar);
Radar_DrawBackground(&radar);

N10_Parser_Init(&n10_parser);
__HAL_UART_ENABLE_IT(&huart2, UART_IT_IDLE);
HAL_UART_Receive_DMA(&huart2, dma_rx_buffer, DMA_RX_BUFFER_SIZE);
```

### 3. 主循环处理
```c
while (1) {
    // 处理DMA接收的数据
    if (dma_rx_flag) {
        dma_rx_flag = 0;
        N10_Parser_ProcessDMA(&n10_parser, dma_rx_buffer, dma_rx_len);
    }

    // 处理解析后的帧
    while (N10_Parser_GetFrame(&n10_parser, &frame)) {
        // 更新雷达数据
        for (int i = 0; i < N10_POINTS_PER_FRAME; i++) {
            uint16_t angle = frame.points[i].angle / 100;  // 转换为度
            Radar_UpdatePoint(&radar, angle, frame.points[i].distance);
        }
        
        // 每累积一圈后绘制 (约30帧=360度)
        static uint8_t frame_count = 0;
        if (++frame_count >= 30) {
            frame_count = 0;
            Radar_Draw(&radar);
            Radar_ClearData(&radar);
        }
    }
}
```

### 4. DMA空闲中断处理
```c
void USART2_IRQHandler(void) {
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_IDLE)) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart2);
        
        // 停止当前DMA
        HAL_UART_DMAStop(&huart2);
        
        // 获取接收到的数据长度
        dma_rx_len = DMA_RX_BUFFER_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart2_rx);
        dma_rx_flag = 1;
        
        // 重新启动DMA
        HAL_UART_Receive_DMA(&huart2, dma_rx_buffer, DMA_RX_BUFFER_SIZE);
    }
}
```

## 性能优化

1. **环形队列缓冲**: 防止数据丢失，支持4帧缓冲
2. **DMA+空闲中断**: 高效串口接收，减少CPU占用
3. **弧线绘制**: 只画边缘弧线，大幅提升绘制速度
4. **背景缓存**: 背景只画一次，减少重复绘制

## 配置调整

- **角度步进**: 修改 `RADAR_ANGLE_STEP` (默认12度)
- **距离阈值**: 修改 `DIST_THRESHOLD_NEAR/MID`
- **队列大小**: 修改 `N10_FRAME_QUEUE_SIZE` (默认4)
- **显示半径**: 修改 `RADAR_RADIUS` (默认100像素)

## 注意事项

1. LCD方向会自动调整角度标注位置
2. 雷达数据每帧16个点，约30帧覆盖360度
3. 队列满时会记录丢失帧数，可通过 `N10_Parser_GetLostFrames()` 监控
4. 模块间低耦合设计，`radar_display` 可用于其他雷达数据源
