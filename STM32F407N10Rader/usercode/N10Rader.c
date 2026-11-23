/**
 * @file N10Rader.c
 * @brief 镭神N10激光雷达串口数据解析库实现
 */

#include "N10Rader.h"
#include <string.h>

/* 私有函数声明 */
static bool N10_VerifyCRC(const uint8_t *data, uint16_t len);
static void N10_ParseFrameData(N10_Parser_t *parser, N10_Frame_t *frame);

/**
 * @brief 初始化解析器
 */
void N10_Parser_Init(N10_Parser_t *parser)
{
    memset(parser, 0, sizeof(N10_Parser_t));
    parser->queue_head = 0;
    parser->queue_tail = 0;
    parser->queue_count = 0;
    parser->lost_frames = 0;
}

/**
 * @brief 接收一个字节数据 (在串口接收中断中调用)
 */
void N10_Parser_RxByte(N10_Parser_t *parser, uint8_t byte)
{
    /* 查找帧头 */
    if (parser->rx_index == 0) {
        if (byte == N10_FRAME_HEADER_0) {
            parser->rx_buffer[0] = byte;
            parser->rx_index = 1;
        }
        return;
    }
    
    if (parser->rx_index == 1) {
        if (byte == N10_FRAME_HEADER_1) {
            parser->rx_buffer[1] = byte;
            parser->rx_index = 2;
        } else if (byte == N10_FRAME_HEADER_0) {
            /* 保持等待第二个帧头字节 */
            parser->rx_buffer[0] = byte;
            parser->rx_index = 1;
        } else {
            /* 重新开始查找 */
            parser->rx_index = 0;
        }
        return;
    }
    
    /* 继续接收数据 */
    parser->rx_buffer[parser->rx_index++] = byte;
    
    /* 检查是否接收完整帧 */
    if (parser->rx_index >= N10_FRAME_LENGTH) {
        /* 验证CRC */
        if (N10_VerifyCRC(parser->rx_buffer, N10_FRAME_LENGTH)) {
            /* 检查队列是否已满 */
            if (parser->queue_count < N10_FRAME_QUEUE_SIZE) {
                /* 解析到队列头位置 */
                N10_ParseFrameData(parser, &parser->frame_queue[parser->queue_head]);
                parser->queue_head = (parser->queue_head + 1) % N10_FRAME_QUEUE_SIZE;
                parser->queue_count++;
            } else {
                /* 队列满，丢失帧 */
                parser->lost_frames++;
            }
        }
        /* 重置接收索引 */
        parser->rx_index = 0;
    }
    
    /* 防止缓冲区溢出 */
    if (parser->rx_index >= N10_RX_BUFFER_SIZE) {
        parser->rx_index = 0;
    }
}

/**
 * @brief 检查是否有新帧就绪
 */
bool N10_Parser_IsFrameReady(N10_Parser_t *parser)
{
    return parser->queue_count > 0;
}

/**
 * @brief 获取解析后的帧数据
 */
bool N10_Parser_GetFrame(N10_Parser_t *parser, N10_Frame_t *frame)
{
    if (parser->queue_count == 0) {
        return false;
    }
    
    /* 从队列尾部取出帧 */
    memcpy(frame, &parser->frame_queue[parser->queue_tail], sizeof(N10_Frame_t));
    parser->queue_tail = (parser->queue_tail + 1) % N10_FRAME_QUEUE_SIZE;
    parser->queue_count--;
    
    return true;
}

/**
 * @brief 获取队列中的帧数量
 */
uint8_t N10_Parser_GetQueueCount(N10_Parser_t *parser)
{
    return parser->queue_count;
}

/**
 * @brief 获取丢失的帧数量
 */
uint32_t N10_Parser_GetLostFrames(N10_Parser_t *parser)
{
    return parser->lost_frames;
}

/**
 * @brief 批量处理DMA接收的数据
 */
void N10_Parser_ProcessDMA(N10_Parser_t *parser, const uint8_t *data, uint16_t len)
{
    for (uint16_t i = 0; i < len; i++) {
        N10_Parser_RxByte(parser, data[i]);
    }
}

/**
 * @brief 角度值转换为实际角度 (度)
 */
float N10_AngleToFloat(uint16_t angle_raw)
{
    return (float)angle_raw / 100.0f;
}

/**
 * @brief 计算转速 (Hz)
 */
float N10_GetRotationHz(uint16_t speed_us)
{
    if (speed_us == 0) {
        return 0.0f;
    }
    /* 一个码盘有24个齿, 转一圈时间 = speed_us * 24 μs */
    float rotation_time_ms = (float)speed_us * 24.0f / 1000.0f;
    return 1000.0f / rotation_time_ms;
}

/**
 * @brief 验证CRC校验
 */
static bool N10_VerifyCRC(const uint8_t *data, uint16_t len)
{
    if (len < N10_FRAME_LENGTH) {
        return false;
    }
    
    uint8_t checksum = 0;
    for (uint16_t i = 0; i < 57; i++) {
        checksum += data[i];
    }
    
    return checksum == data[57];
}

/**
 * @brief 解析帧数据
 */
static void N10_ParseFrameData(N10_Parser_t *parser, N10_Frame_t *frame)
{
    uint8_t *data = parser->rx_buffer;
    
    /* 解析转速信息 (Byte 3-4, 高位在前) */
    frame->speed_us = ((uint16_t)data[3] << 8) | data[4];
    
    /* 解析起始角度 (Byte 5-6, 高位在前) */
    frame->start_angle = ((uint16_t)data[5] << 8) | data[6];
    
    /* 解析结束角度 (Byte 55-56, 高位在前) */
    frame->stop_angle = ((uint16_t)data[55] << 8) | data[56];
    
    /* 计算角度步进 (使用整数运算避免浮点) */
    int32_t angle_diff = (int32_t)frame->stop_angle - (int32_t)frame->start_angle;
    if (angle_diff < 0) {
        angle_diff += 36000;  /* 360度 * 100 */
    }
    int32_t angle_step = angle_diff / (N10_POINTS_PER_FRAME - 1);
    
    /* 解析16个点的数据 (Byte 7-54) */
    for (uint8_t i = 0; i < N10_POINTS_PER_FRAME; i++) {
        uint8_t offset = 7 + i * 3;
        
        /* 距离 (2字节, 高位在前) */
        frame->points[i].distance = ((uint16_t)data[offset] << 8) | data[offset + 1];
        
        /* 强度 (1字节) */
        frame->points[i].intensity = data[offset + 2];
        
        /* 角度 */
        int32_t angle = (int32_t)frame->start_angle + i * angle_step;
        if (angle >= 36000) {
            angle -= 36000;
        }
        frame->points[i].angle = (uint16_t)angle;
    }
    
    frame->valid = 1;
}
