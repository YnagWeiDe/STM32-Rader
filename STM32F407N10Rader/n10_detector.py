"""
镭神N10激光雷达 - 角度范围距离检测脚本
功能：检测指定角度范围内的距离，根据阈值发送ON/OFF信号
"""

import serial
import time
import math
from dataclasses import dataclass
from typing import List, Optional


@dataclass
class LidarPoint:
    angle: float
    distance: float
    intensity: int


@dataclass
class LidarFrame:
    timestamp: float
    speed_us: int
    rpm: float
    start_angle: float
    stop_angle: float
    points: List[LidarPoint]


class N10LidarReader:
    """N10激光雷达串口读取器"""
    
    FRAME_HEADER = bytes([0xA5, 0x5A])
    FRAME_LENGTH = 58
    POINTS_PER_FRAME = 16
    
    def __init__(self, port: str, baudrate: int = 115200):
        self.port = port
        self.baudrate = baudrate
        self.serial: Optional[serial.Serial] = None
        self.buffer = bytearray()
        
    def connect(self) -> bool:
        try:
            self.serial = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                bytesize=serial.EIGHTBITS,
                parity=serial.PARITY_NONE,
                stopbits=serial.STOPBITS_ONE,
                timeout=1
            )
            return True
        except serial.SerialException as e:
            print(f"雷达串口连接失败: {e}")
            return False
    
    def disconnect(self):
        if self.serial and self.serial.is_open:
            self.serial.close()
    
    def _verify_crc(self, data: bytes) -> bool:
        if len(data) < self.FRAME_LENGTH:
            return False
        checksum = sum(data[:57]) & 0xFF
        return checksum == data[57]
    
    def _parse_frame(self, data: bytes) -> Optional[LidarFrame]:
        if len(data) < self.FRAME_LENGTH or data[0:2] != self.FRAME_HEADER:
            return None
        if not self._verify_crc(data):
            return None
        
        speed_us = (data[3] << 8) | data[4]
        rotation_time_ms = speed_us * 24 / 1000.0
        rpm = 1000.0 / rotation_time_ms if rotation_time_ms > 0 else 0
        
        start_angle = ((data[5] << 8) | data[6]) / 100.0
        stop_angle = ((data[55] << 8) | data[56]) / 100.0
        
        angle_diff = stop_angle - start_angle
        if angle_diff < 0:
            angle_diff += 360.0
        angle_step = angle_diff / (self.POINTS_PER_FRAME - 1) if self.POINTS_PER_FRAME > 1 else 0
        
        points = []
        for i in range(self.POINTS_PER_FRAME):
            offset = 7 + i * 3
            distance = (data[offset] << 8) | data[offset + 1]
            intensity = data[offset + 2]
            angle = start_angle + i * angle_step
            if angle >= 360.0:
                angle -= 360.0
            points.append(LidarPoint(angle=angle, distance=distance, intensity=intensity))
        
        return LidarFrame(
            timestamp=time.time(),
            speed_us=speed_us,
            rpm=rpm,
            start_angle=start_angle,
            stop_angle=stop_angle,
            points=points
        )
    
    def read_frame(self) -> Optional[LidarFrame]:
        if not self.serial or not self.serial.is_open:
            return None
        
        if self.serial.in_waiting > 0:
            self.buffer.extend(self.serial.read(self.serial.in_waiting))
        
        while len(self.buffer) >= self.FRAME_LENGTH:
            header_pos = self.buffer.find(self.FRAME_HEADER)
            if header_pos == -1:
                self.buffer = self.buffer[-1:] if self.buffer else bytearray()
                return None
            if header_pos > 0:
                self.buffer = self.buffer[header_pos:]
            
            if len(self.buffer) >= self.FRAME_LENGTH:
                frame_data = bytes(self.buffer[:self.FRAME_LENGTH])
                frame = self._parse_frame(frame_data)
                if frame:
                    self.buffer = self.buffer[self.FRAME_LENGTH:]
                    return frame
                else:
                    self.buffer = self.buffer[2:]
            else:
                break
        return None


class DistanceDetector:
    """距离检测器 - 检测360度范围内的目标，每10度步进"""
    
    def __init__(self, distance_threshold: float, angle_step: int = 10,
                 output_port: str = None, output_baudrate: int = 9600):
        """
        Args:
            distance_threshold: 距离阈值 (mm)，小于等于此值为ON
            angle_step: 角度步进 (度)，默认10度
            output_port: 输出串口 (可选，None则只打印)
            output_baudrate: 输出串口波特率
        """
        self.distance_threshold = distance_threshold
        self.angle_step = angle_step
        self.output_port = output_port
        self.output_baudrate = output_baudrate
        self.output_serial: Optional[serial.Serial] = None
        
        # 360度数据缓存 (按角度分区)
        self.scan_data = {}  # {angle_bin: (distance, intensity)}
        self.last_start_angle = None
        self.last_send_time = 0
        self.send_interval = 0.5  # 发送间隔(秒)
    
    def connect_output(self) -> bool:
        """连接输出串口"""
        if not self.output_port:
            return True
        try:
            self.output_serial = serial.Serial(
                port=self.output_port,
                baudrate=self.output_baudrate,
                timeout=1
            )
            print(f"输出串口 {self.output_port} 已连接")
            return True
        except serial.SerialException as e:
            print(f"输出串口连接失败: {e}")
            return False
    
    def disconnect_output(self):
        if self.output_serial and self.output_serial.is_open:
            self.output_serial.close()
    
    def process_frame(self, frame: LidarFrame):
        """处理一帧数据，累积360度数据"""
        # 检测是否开始新的一圈 (角度回到起点)
        if self.last_start_angle is not None:
            # 如果当前帧起始角度 < 上一帧起始角度，说明转了一圈
            if frame.start_angle < self.last_start_angle and self.last_start_angle > 300:
                self._output_scan_data()
                self.scan_data.clear()
        
        self.last_start_angle = frame.start_angle
        
        # 累积当前帧数据，按步进分组
        for point in frame.points:
            if point.distance > 0:
                # 按步进分组，如步进10: 0-9度归为0, 10-19度归为10
                angle_bin = (int(point.angle) // self.angle_step) * self.angle_step
                # 保留该区间最近的点
                if angle_bin not in self.scan_data or point.distance < self.scan_data[angle_bin][0]:
                    self.scan_data[angle_bin] = (point.distance, point.angle, point.intensity)
    
    def _output_scan_data(self):
        """输出一圈数据"""
        current_time = time.time()
        if current_time - self.last_send_time < self.send_interval:
            return
        self.last_send_time = current_time
        
        if not self.scan_data:
            return
        
        # 生成所有步进角度 (0, 10, 20, ..., 350)
        all_angles = list(range(0, 360, self.angle_step))
        
        # 构建输出数据
        output_lines = []
        detected_count = 0
        
        for angle_bin in all_angles:
            if angle_bin in self.scan_data:
                distance, exact_angle, intensity = self.scan_data[angle_bin]
            else:
                distance = 0  # 无数据
            
            is_detected = distance > 0 and distance <= self.distance_threshold
            state = "ON" if is_detected else "OFF"
            if is_detected:
                detected_count += 1
            
            # 消息格式: 角度,距离,状态
            output_lines.append(f"{angle_bin},{distance},{state}")
        
        # 发送到串口: 一行一个点
        message = "\n".join(output_lines) + "\n---\n"  # ---表示一圈结束
        
        if self.output_serial and self.output_serial.is_open:
            self.output_serial.write(message.encode('utf-8'))
        
        # 控制台显示
        print(f"\n[扫描完成] 步进: {self.angle_step}° | 点数: {len(all_angles)} | 检测到(ON): {detected_count} | 阈值: {self.distance_threshold}mm")
        
        # 显示所有点
        for angle_bin in all_angles:
            if angle_bin in self.scan_data:
                distance, exact_angle, intensity = self.scan_data[angle_bin]
            else:
                distance = 0
            is_detected = distance > 0 and distance <= self.distance_threshold
            state = "ON" if is_detected else "OFF"
            status_icon = "🔴" if is_detected else "⚪"
            print(f"{status_icon} {state} | 角度: {angle_bin:3}° | 距离: {distance:5}mm")


def run_detector(lidar_port: str, distance_threshold: float, angle_step: int = 10, output_port: str = None):
    """
    运行检测器
    
    Args:
        lidar_port: 雷达串口 (如 COM15)
        distance_threshold: 距离阈值(mm)
        angle_step: 角度步进 (度)
        output_port: 输出串口 (可选)
    """
    print("=" * 50)
    print("N10激光雷达 - 360度距离检测器")
    print("=" * 50)
    print(f"雷达串口: {lidar_port}")
    print(f"检测范围: 360° (步进{angle_step}°)")
    print(f"距离阈值: {distance_threshold}mm")
    print(f"输出串口: {output_port or '无(仅控制台)'}")
    print("=" * 50)
    
    # 初始化
    reader = N10LidarReader(lidar_port)
    detector = DistanceDetector(distance_threshold, angle_step, output_port)
    
    if not reader.connect():
        return
    
    if output_port and not detector.connect_output():
        reader.disconnect()
        return
    
    print("\n开始检测... (按Ctrl+C停止)\n")
    
    try:
        while True:
            frame = reader.read_frame()
            if frame:
                detector.process_frame(frame)
            time.sleep(0.001)
    except KeyboardInterrupt:
        print("\n\n停止检测")
    finally:
        reader.disconnect()
        detector.disconnect_output()


if __name__ == "__main__":
    # ============ 配置参数 ============
    LIDAR_PORT = "COM13"        # 雷达串口
    DISTANCE_THRESHOLD = 200    # 距离阈值 (mm)，小于等于此值发送ON
    ANGLE_STEP = 12             # 角度步进 (度)，如12表示0,12,24...348 (共30个点)
    
    OUTPUT_PORT = None          # 输出串口，None则只打印到控制台
    # OUTPUT_PORT = "COM3"      # 如需发送到其他设备，设置串口号
    # ================================
    
    run_detector(
        lidar_port=LIDAR_PORT,
        distance_threshold=DISTANCE_THRESHOLD,
        angle_step=ANGLE_STEP,
        output_port=OUTPUT_PORT
    )
