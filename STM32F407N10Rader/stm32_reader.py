"""
读取STM32转发的N10雷达数据
串口格式: 115200 bps
数据格式:
  转速Hz,起始角度,结束角度
  角度,距离mm,强度
  ...（16个点）
"""

import serial
import time
from dataclasses import dataclass
from typing import List, Optional


@dataclass
class LidarPoint:
    angle: float
    distance: int
    intensity: int


@dataclass
class LidarFrame:
    rpm: float
    start_angle: float
    stop_angle: float
    points: List[LidarPoint]


class STM32LidarReader:
    """读取STM32转发的雷达数据"""
    
    POINTS_PER_FRAME = 16
    
    def __init__(self, port: str, baudrate: int = 115200):
        self.port = port
        self.baudrate = baudrate
        self.serial: Optional[serial.Serial] = None
    
    def connect(self) -> bool:
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=1
            )
            print(f"已连接到 {self.port} @ {self.baudrate}")
            return True
        except Exception as e:
            print(f"连接失败: {e}")
            return False
    
    def disconnect(self):
        if self.serial and self.serial.is_open:
            self.serial.close()
            print("已断开连接")
    
    def is_header_line(self, parts: list) -> bool:
        """判断是否为帧头行（转速,起始角度,结束角度）"""
        if len(parts) != 3:
            return False
        try:
            # 帧头行特征: 第一个值是转速(约10Hz), 第二三个值是角度(有小数)
            val1 = float(parts[0])
            val2 = float(parts[1])
            # 转速通常是10左右，角度是0-360
            # 点数据的距离通常是整数且较大(>50)
            if val1 < 20 and '.' in parts[1]:
                return True
        except:
            pass
        return False
    
    def read_frame(self) -> Optional[LidarFrame]:
        """读取一帧数据"""
        if not self.serial or not self.serial.is_open:
            return None
        
        try:
            points = []
            rpm = 0.0
            start_angle = 0.0
            stop_angle = 0.0
            
            # 持续读取直到找到帧头
            while True:
                line = self.serial.readline().decode('utf-8').strip()
                if not line:
                    continue
                
                parts = line.split(',')
                if len(parts) != 3:
                    continue
                
                # 检测帧头行
                if self.is_header_line(parts):
                    # 如果之前已经有点数据，说明上一帧完成
                    if points:
                        return LidarFrame(
                            rpm=rpm,
                            start_angle=start_angle,
                            stop_angle=stop_angle,
                            points=points
                        )
                    # 记录新帧的帧头
                    rpm = float(parts[0])
                    start_angle = float(parts[1])
                    stop_angle = float(parts[2])
                    points = []
                else:
                    # 点数据行: 角度,距离,强度
                    try:
                        angle = float(parts[0])
                        distance = int(parts[1])
                        intensity = int(parts[2])
                        points.append(LidarPoint(angle, distance, intensity))
                    except ValueError:
                        continue
        
        except Exception as e:
            print(f"读取错误: {e}")
            return None


def main():
    # ============ 配置参数 ============
    PORT = "COM13"
    BAUDRATE = 115200
    DISTANCE_THRESHOLD = 200  # 距离阈值(mm)
    # ================================
    
    reader = STM32LidarReader(PORT, BAUDRATE)
    
    if not reader.connect():
        return
    
    print("=" * 50)
    print("STM32 N10雷达数据接收器")
    print(f"端口: {PORT} @ {BAUDRATE}")
    print(f"距离阈值: {DISTANCE_THRESHOLD}mm")
    print("=" * 50)
    print("按 Ctrl+C 停止\n")
    
    try:
        frame_count = 0
        while True:
            frame = reader.read_frame()
            if frame:
                frame_count += 1
                
                # 显示帧信息
                print(f"\n[帧 {frame_count}] 转速: {frame.rpm:.1f}Hz | 角度: {frame.start_angle:.1f}° - {frame.stop_angle:.1f}°")
                
                # 显示每个点
                for i, point in enumerate(frame.points):
                    is_near = point.distance > 0 and point.distance <= DISTANCE_THRESHOLD
                    status = "🔴 ON " if is_near else "⚪ OFF"
                    print(f"  {status} | 角度: {point.angle:6.2f}° | 距离: {point.distance:5}mm | 强度: {point.intensity:3}")
                
                # 间隔1秒
                time.sleep(1)
    
    except KeyboardInterrupt:
        print("\n\n已停止")
    finally:
        reader.disconnect()


if __name__ == "__main__":
    main()
