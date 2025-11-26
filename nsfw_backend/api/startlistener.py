import time
import serial
import subprocess
from django.core.management.base import BaseCommand
from api import views as api_views

# --- Mac 控制函式 ---
def set_brightness(level):
    """設定螢幕亮度 (0.0 to 1.0)"""
    # 這個方法需要安裝額外的工具，我們先用 osascript 模擬按鍵
    # For now, we use key codes. 16 steps total.
    # First, decrease to 0, then increase to the target level.
    decrease_command = ['osascript', '-e', 'tell application "System Events" to repeat 16 times', '-e', 'key code 145', '-e', 'end repeat']
    subprocess.run(decrease_command, check=False)
    
    if level > 0:
        increase_command = ['osascript', '-e', f'tell application "System Events" to repeat {level} times', '-e', 'key code 144', '-e', 'end repeat']
        subprocess.run(increase_command, check=False)

def set_volume(level):
    """設定系統音量 (0-100)"""
    subprocess.run(['osascript', '-e', f'set volume output volume {level}'], check=False)

def open_app(app_name):
    """開啟指定的應用程式"""
    subprocess.run(['open', '-a', app_name], check=False)

class Command(BaseCommand):
    help = 'Starts the serial listener for the ultrasonic sensor'

    def handle(self, *args, **kwargs):
        self.stdout.write("正在啟動序列通訊監聽器...")
        
        serial_port = '/dev/cu.usbserial-BG02MH6B' 
        baud_rate = 9600
        current_state = None

        while True:
            try:
                self.stdout.write(f"嘗試連接序列埠 {serial_port}...")
                ser = serial.Serial(serial_port, baud_rate, timeout=2)
                api_views.serial_port_object = ser
                self.stdout.write(self.style.SUCCESS("序列埠連接成功！"))

                while ser.is_open:
                    line = ser.readline().decode('utf-8').strip()
                    if not line: continue

                    # 更新全域狀態，讓 StatusView 可以讀取
                    api_views.current_distance = line

                    # 解析距離並決定狀態
                    try:
                        # 解析 "DIST:xxx" 格式
                        distance = int(line.split(":")[1])
                        self.stdout.write(f"偵測到距離: {distance} cm")
                        
                        new_state = None
                        if distance <= api_views.app_settings.get('danger_threshold', 50):
                            new_state = 'danger'
                        elif distance <= api_views.app_settings.get('warning_threshold', 100):
                            new_state = 'warning'
                        else:
                            new_state = 'safe'

                        # 如果狀態改變，就執行對應的動作
                        if new_state != current_state:
                            current_state = new_state
                            self.stdout.write(f"狀態改變 -> {current_state.upper()}")
                            
                            config = api_views.app_settings[current_state]
                            set_volume(config['volume'])
                            set_brightness(config['brightness'])
                            if current_state == 'danger':
                                open_app(config['target_app'])

                    except (IndexError, ValueError):
                        continue # 忽略格式不正確的行

            except serial.SerialException:
                self.stdout.write(self.style.ERROR(f"無法連接序列埠，5秒後重試..."))
                if 'ser' in locals() and ser.is_open: ser.close()
                api_views.serial_port_object = None
                time.sleep(5)
            except KeyboardInterrupt:
                self.stdout.write("監聽器已停止。")
                if 'ser' in locals() and ser.is_open: ser.close()
                break