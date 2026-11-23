/**
 * @file N10Rader.h
 * @brief 镭神N10激光雷达串口数据解析库
 * @note 波特率: 230400 bps
 * 
 * 数据帧格式 (58字节):
 * - Byte 0-1:   帧头 0xA5 0x5A
 * - Byte 2:     帧长度
 * - Byte 3-4:   转速信息 (高位在前, 单位μs)
 * - Byte 5-6:   起始角度 (高位在前, 实际角度*100)
 * - Byte 7-54:  点云数据 (16个点, 每点3字节: 2字节距离 + 1字节强度)
 * - Byte 55-56: 结束角度
 * - Byte 57:    CRC校验 (Byte0~56累加和)
 */

#ifndef __N10RADER_H
#define __N10RADER_H

#include <stdint.h>
#include <stdbool.h>

/* 常量定义 */
#define N10_FRAME_HEADER_0      0xA5
#define N10_FRAME_HEADER_1      0x5A
#define N10_FRAME_LENGTH        58
#define N10_POINTS_PER_FRAME    16
#define N10_RX_BUFFER_SIZE      128
#define N10_DMA_RX_BUFFER_SIZE  256
#define N10_FRAME_QUEUE_SIZE    4       // 帧缓冲队列大小

/* 单个激光点数据 */
typedef struct {
    uint16_t angle;      // 角度 (实际角度*100, 如 27020 表示 270.20°)
    uint16_t distance;   // 距离 (mm)
    uint8_t  intensity;  // 强度
} N10_Point_t;

/* 一帧激光数据 */
typedef struct {
    uint16_t speed_us;                      // 转速信息 (μs)
    uint16_t start_angle;                   // 起始角度 (*100)
    uint16_t stop_angle;                    // 结束角度 (*100)
    N10_Point_t points[N10_POINTS_PER_FRAME]; // 16个点
    uint8_t  valid;                         // 帧有效标志
} N10_Frame_t;

/* 解析器状态 */
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

/**
 * @brief 初始化解析器
 * @param parser 解析器指针
 */
void N10_Parser_Init(N10_Parser_t *parser);

/**
 * @brief 接收一个字节数据 (在串口接收中断中调用)
 * @param parser 解析器指针
 * @param byte 接收到的字节
 */
void N10_Parser_RxByte(N10_Parser_t *parser, uint8_t byte);

/**
 * @brief 检查是否有新帧就绪
 * @param parser 解析器指针
 * @return true: 有新帧, false: 无新帧
 */
bool N10_Parser_IsFrameReady(N10_Parser_t *parser);

/**
 * @brief 获取解析后的帧数据
 * @param parser 解析器指针
 * @param frame 输出帧数据
 * @return true: 获取成功, false: 无有效帧
 */
bool N10_Parser_GetFrame(N10_Parser_t *parser, N10_Frame_t *frame);

/**
 * @brief 获取队列中的帧数量
 * @param parser 解析器指针
 * @return 队列中等待处理的帧数量
 */
uint8_t N10_Parser_GetQueueCount(N10_Parser_t *parser);

/**
 * @brief 获取丢失的帧数量
 * @param parser 解析器指针
 * @return 因队列满而丢失的帧数量
 */
uint32_t N10_Parser_GetLostFrames(N10_Parser_t *parser);

/**
 * @brief 批量处理DMA接收的数据
 * @param parser 解析器指针
 * @param data 数据指针
 * @param len 数据长度
 */
void N10_Parser_ProcessDMA(N10_Parser_t *parser, const uint8_t *data, uint16_t len);

/**
 * @brief 角度值转换为实际角度 (度)
 * @param angle_raw 原始角度值 (*100)
 * @return 实际角度 (度), 如 270.20
 */
float N10_AngleToFloat(uint16_t angle_raw);

/**
 * @brief 计算转速 (Hz)
 * @param speed_us 转速信息 (μs)
 * @return 转速 (Hz)
 */
float N10_GetRotationHz(uint16_t speed_us);

#endif /* __N10RADER_H */
