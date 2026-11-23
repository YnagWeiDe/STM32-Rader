/**
 * @file radar_display.c
 * @brief N10雷达LCD显示模块实现
 */

#include "radar_display.h"
#include "gui_utils.h"
#include <math.h>
#include <string.h>

/* 预计算的sin/cos表 (每度一个值, *1000) */
static const int16_t sin_table[360] = {
    0, 17, 35, 52, 70, 87, 105, 122, 139, 156, 174, 191, 208, 225, 242, 259,
    276, 292, 309, 326, 342, 358, 375, 391, 407, 423, 438, 454, 469, 485, 500, 515,
    530, 545, 559, 574, 588, 602, 616, 629, 643, 656, 669, 682, 695, 707, 719, 731,
    743, 755, 766, 777, 788, 799, 809, 819, 829, 839, 848, 857, 866, 875, 883, 891,
    899, 906, 914, 921, 927, 934, 940, 946, 951, 956, 961, 966, 970, 974, 978, 982,
    985, 988, 990, 993, 995, 996, 998, 999, 999, 1000, 1000, 1000, 999, 999, 998, 996,
    995, 993, 990, 988, 985, 982, 978, 974, 970, 966, 961, 956, 951, 946, 940, 934,
    927, 921, 914, 906, 899, 891, 883, 875, 866, 857, 848, 839, 829, 819, 809, 799,
    788, 777, 766, 755, 743, 731, 719, 707, 695, 682, 669, 656, 643, 629, 616, 602,
    588, 574, 559, 545, 530, 515, 500, 485, 469, 454, 438, 423, 407, 391, 375, 358,
    342, 326, 309, 292, 276, 259, 242, 225, 208, 191, 174, 156, 139, 122, 105, 87,
    70, 52, 35, 17, 0, -17, -35, -52, -70, -87, -105, -122, -139, -156, -174, -191,
    -208, -225, -242, -259, -276, -292, -309, -326, -342, -358, -375, -391, -407, -423, -438, -454,
    -469, -485, -500, -515, -530, -545, -559, -574, -588, -602, -616, -629, -643, -656, -669, -682,
    -695, -707, -719, -731, -743, -755, -766, -777, -788, -799, -809, -819, -829, -839, -848, -857,
    -866, -875, -883, -891, -899, -906, -914, -921, -927, -934, -940, -946, -951, -956, -961, -966,
    -970, -974, -978, -982, -985, -988, -990, -993, -995, -996, -998, -999, -999, -1000, -1000, -1000,
    -999, -999, -998, -996, -995, -993, -990, -988, -985, -982, -978, -974, -970, -966, -961, -956,
    -951, -946, -940, -934, -927, -921, -914, -906, -899, -891, -883, -875, -866, -857, -848, -839,
    -829, -819, -809, -799, -788, -777, -766, -755, -743, -731, -719, -707, -695, -682, -669, -656,
    -643, -629, -616, -602, -588, -574, -559, -545, -530, -515, -500, -485, -469, -454, -438, -423,
    -407, -391, -375, -358, -342, -326, -309, -292, -276, -259, -242, -225, -208, -191, -174, -156,
    -139, -122, -105, -87, -70, -52, -35, -17
};

/* cos = sin(angle + 90) */
static int16_t fast_sin(int16_t angle) {
    while (angle < 0) angle += 360;
    while (angle >= 360) angle -= 360;
    return sin_table[angle];
}

static int16_t fast_cos(int16_t angle) {
    return fast_sin(angle + 90);
}

/**
 * @brief 初始化雷达显示器
 */
void Radar_Init(RadarDisplay_t *radar)
{
    radar->center_x = RADAR_CENTER_X;
    radar->center_y = RADAR_CENTER_Y;
    radar->radius = RADAR_RADIUS;
    radar->max_distance = RADAR_MAX_DISTANCE;
    radar->angle_step = RADAR_ANGLE_STEP;
    radar->near_threshold = DIST_THRESHOLD_NEAR;
    radar->mid_threshold = DIST_THRESHOLD_MID;
    
    Radar_ClearData(radar);
}

/**
 * @brief 清空雷达数据
 */
void Radar_ClearData(RadarDisplay_t *radar)
{
    for (int i = 0; i < RADAR_SECTOR_COUNT; i++) {
        radar->sectors[i].min_distance = 0xFFFF;
        radar->sectors[i].has_data = 0;
    }
}

/**
 * @brief 根据距离获取颜色
 */
uint16_t Radar_GetColorByDistance(uint16_t distance, uint16_t near_threshold, uint16_t mid_threshold)
{
    if (distance == 0 || distance == 0xFFFF) {
        return GRAY;  // 无数据
    }
    if (distance <= near_threshold) {
        return RED;   // 近距离 - 危险
    }
    if (distance <= mid_threshold) {
        return YELLOW; // 中距离 - 警告
    }
    return GREEN;     // 远距离 - 安全
}

/**
 * @brief 更新单个点数据
 */
void Radar_UpdatePoint(RadarDisplay_t *radar, uint16_t angle_deg, uint16_t distance)
{
    if (distance == 0) return;  // 无效数据跳过
    
    // 角度归一化到 0-359
    if (angle_deg >= 360) angle_deg -= 360;
    
    // 计算扇区索引
    uint16_t sector_idx = angle_deg / radar->angle_step;
    if (sector_idx >= RADAR_SECTOR_COUNT) sector_idx = RADAR_SECTOR_COUNT - 1;
    
    // 更新该扇区的最小距离
    if (distance < radar->sectors[sector_idx].min_distance) {
        radar->sectors[sector_idx].min_distance = distance;
    }
    radar->sectors[sector_idx].has_data = 1;
}

/**
 * @brief 绘制雷达背景
 */
void Radar_DrawBackground(RadarDisplay_t *radar)
{
    uint16_t cx = radar->center_x;
    uint16_t cy = radar->center_y;
    uint16_t r = radar->radius;
    uint8_t dir = GetLcdDir();  // 获取LCD方向
    
    // 根据方向获取屏幕尺寸
    uint16_t screen_w = (dir == 0 || dir == 2) ? LCD_WIDTH : LCD_HEIGHT;
    uint16_t screen_h = (dir == 0 || dir == 2) ? LCD_HEIGHT : LCD_WIDTH;
    
    // 清除雷达区域
    GUI_FillRect(cx - r - 15, cy - r - 15, r * 2 + 30, r * 2 + 30, BLACK);
    
    // 画同心圆 (刻度圈)
    for (int ring = 1; ring <= 3; ring++) {
        uint16_t ring_r = r * ring / 3;
        for (int angle = 0; angle < 360; angle += 2) {
            int16_t x = cx + (ring_r * fast_cos(angle)) / 1000;
            int16_t y = cy - (ring_r * fast_sin(angle)) / 1000;
            if (x >= 0 && x < screen_w && y >= 0 && y < screen_h) {
                LCD_DrawPoint(x, y, GRAY);
            }
        }
    }
    
    // 画十字线
    LCD_DrawLine(cx - r, cy, cx + r, cy, GRAY);
    LCD_DrawLine(cx, cy - r, cx, cy + r, GRAY);
    
    // 根据LCD方向绘制角度标注
    // dir: 0=竖屏, 1=横屏, 2=竖屏180°, 3=横屏180°
    const char *labels[4];
    switch (dir) {
        case 0:  // 竖屏: 上0 右90 下180 左270
            labels[0] = "0"; labels[1] = "90"; labels[2] = "180"; labels[3] = "270";
            break;
        case 1:  // 横屏: 上270 右0 下90 左180
            labels[0] = "270"; labels[1] = "0"; labels[2] = "90"; labels[3] = "180";
            break;
        case 2:  // 竖屏180°: 上180 右270 下0 左90
            labels[0] = "180"; labels[1] = "270"; labels[2] = "0"; labels[3] = "90";
            break;
        case 3:  // 横屏180°: 上90 右180 下270 左0
            labels[0] = "90"; labels[1] = "180"; labels[2] = "270"; labels[3] = "0";
            break;
        default:
            labels[0] = "0"; labels[1] = "90"; labels[2] = "180"; labels[3] = "270";
            break;
    }
    
    // 上右下左位置绘制标注
    GUI_DrawText(cx - 8, cy - r - 12, labels[0], WHITE, BLACK);   // 上
    GUI_DrawText(cx + r + 3, cy - 4, labels[1], WHITE, BLACK);    // 右
    GUI_DrawText(cx - 8, cy + r + 3, labels[2], WHITE, BLACK);    // 下
    GUI_DrawText(cx - r - 24, cy - 4, labels[3], WHITE, BLACK);   // 左
}

/**
 * @brief 绘制扇区弧线
 */
static void Radar_DrawSectorArc(RadarDisplay_t *radar, uint16_t sector_idx, uint16_t color, uint16_t thickness)
{
    uint16_t cx = radar->center_x;
    uint16_t cy = radar->center_y;
    uint16_t r = radar->radius;
    
    int16_t start_angle = sector_idx * radar->angle_step;
    int16_t end_angle = start_angle + radar->angle_step;
    
    // 画弧线 (用点描绘)
    for (int16_t angle = start_angle; angle <= end_angle; angle++) {
        int16_t cos_val = fast_cos(angle);
        int16_t sin_val = fast_sin(angle);
        
        // 画粗弧线 (thickness层)
        for (uint16_t t = 0; t < thickness; t++) {
            uint16_t r_offset = r - t * 3;  // 每层间隔3像素
            if (r_offset < 10) break;
            
            int16_t x = cx + (r_offset * cos_val) / 1000;
            int16_t y = cy - (r_offset * sin_val) / 1000;
            
            if (x >= 0 && x < lcddev.width && y >= 0 && y < lcddev.height) {
                LCD_DrawPoint(x, y, color);
            }
        }
    }
}

/**
 * @brief 绘制雷达图
 */
void Radar_Draw(RadarDisplay_t *radar)
{
    // 画每个扇区的弧线 (统一3层粗细)
    for (int i = 0; i < RADAR_SECTOR_COUNT; i++) {
        if (!radar->sectors[i].has_data) continue;
        
        uint16_t color = Radar_GetColorByDistance(
            radar->sectors[i].min_distance,
            radar->near_threshold,
            radar->mid_threshold
        );
        
        Radar_DrawSectorArc(radar, i, color, 3);  // 统一中弧线3层
    }
}
