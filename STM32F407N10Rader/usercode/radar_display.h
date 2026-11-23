/**
 * @file radar_display.h
 * @brief N10雷达LCD显示模块
 */

#ifndef __RADAR_DISPLAY_H__
#define __RADAR_DISPLAY_H__

#include <stdint.h>
#include "ili9341.h"

/* 雷达显示配置 */
#define RADAR_CENTER_X      120     // 圆心X (240/2)
#define RADAR_CENTER_Y      120     // 圆心Y
#define RADAR_RADIUS        100     // 雷达显示半径(像素)
#define RADAR_MAX_DISTANCE  3000    // 最大显示距离(mm)

/* 距离阈值配置 (可调) */
#define DIST_THRESHOLD_NEAR   100   // 近距离阈值(mm) - 红色
#define DIST_THRESHOLD_MID    300   // 中距离阈值(mm) - 黄色
                                    // 超过中距离 - 绿色

/* 角度步进配置 */
#define RADAR_ANGLE_STEP      12    // 角度步进(度)
#define RADAR_SECTOR_COUNT    (360 / RADAR_ANGLE_STEP)  // 扇区数量

/* 扇区数据结构 */
typedef struct {
    uint16_t min_distance;    // 该扇区最小距离(mm)
    uint8_t  has_data;        // 是否有数据
} RadarSector_t;

/* 雷达显示器结构 */
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

/**
 * @brief 初始化雷达显示器
 * @param radar 雷达显示器指针
 */
void Radar_Init(RadarDisplay_t *radar);

/**
 * @brief 清空雷达数据
 * @param radar 雷达显示器指针
 */
void Radar_ClearData(RadarDisplay_t *radar);

/**
 * @brief 更新单个点数据
 * @param radar 雷达显示器指针
 * @param angle_deg 角度 (0-359度)
 * @param distance 距离 (mm)
 */
void Radar_UpdatePoint(RadarDisplay_t *radar, uint16_t angle_deg, uint16_t distance);

/**
 * @brief 绘制雷达图
 * @param radar 雷达显示器指针
 */
void Radar_Draw(RadarDisplay_t *radar);

/**
 * @brief 绘制雷达背景（刻度圈等）
 * @param radar 雷达显示器指针
 */
void Radar_DrawBackground(RadarDisplay_t *radar);

/**
 * @brief 根据距离获取颜色
 * @param distance 距离(mm)
 * @param near_threshold 近距离阈值
 * @param mid_threshold 中距离阈值
 * @return RGB565颜色
 */
uint16_t Radar_GetColorByDistance(uint16_t distance, uint16_t near_threshold, uint16_t mid_threshold);

#endif /* __RADAR_DISPLAY_H__ */
