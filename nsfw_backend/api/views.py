import subprocess
import os
from rest_framework.views import APIView
from rest_framework.response import Response
from rest_framework import status
import serial

# --- 全域變數，用於在 Django 的不同部分之間共享狀態 ---
# 新的設定結構，包含三個狀態
app_settings = {
    "safe": {"volume": 80, "brightness": 16},
    "warning": {"volume": 40, "brightness": 8},
    "danger": {"volume": 0, "brightness": 0, "target_app": "Microsoft Excel"}
}
current_distance = "N/A"
serial_port_object = None


class SettingsView(APIView):
    """
    讀取和更新三個狀態的設定
    """
    def get(self, request, format=None):
        return Response(app_settings)

    def post(self, request, format=None):
        global app_settings
        data = request.data
        # 更新 app_settings，如果前端傳來的資料不完整，則保留舊值
        app_settings.update(data)
        print(f"設定已更新: {app_settings}")
        return Response(app_settings, status=status.HTTP_200_OK)

class HardwareControlView(APIView):
    """
    控制硬體 (LED/Buzzer)
    """
    def post(self, request, device, format=None):
        global serial_port_object
        # 這個功能暫時與新的三階段邏輯脫鉤，但 API 依然保留
        cmd = ""
        if device == 'led': cmd = "LED_TOGGLE\n"
        elif device == 'buzzer': cmd = "BUZZ_TOGGLE\n"
        else: return Response({"error": "Invalid device"}, status=status.HTTP_404_NOT_FOUND)

        if serial_port_object and serial_port_object.is_open:
            serial_port_object.write(cmd.encode('utf-8'))
            return Response({"status": f"{device} toggled"})
        return Response({"error": "Serial port not connected"}, status=status.HTTP_503_SERVICE_UNAVAILABLE)

class StatusView(APIView):
    """
    獲取目前感測器狀態
    """
    def get(self, request, format=None):
        # 解析從硬體傳來的 "DIST:xxx" 格式
        try:
            dist_str = current_distance.split(":")[1]
            min_dist = int(dist_str)
            return Response({"distance": min_dist})
        except (IndexError, ValueError):
            return Response({"distance": "N/A"})

class ApplicationsView(APIView):
    """
    獲取 macOS 上的應用程式列表
    """
    def get(self, request, format=None):
        try:
            command = ['mdfind', "kMDItemKind == 'Application'"]
            result = subprocess.run(command, capture_output=True, text=True, check=True, encoding='utf-8')
            paths = result.stdout.strip().split('\n')
            app_names = sorted(list(set(os.path.basename(p)[:-4] for p in paths if p.endswith('.app') and os.path.basename(p)[:-4])))
            return Response(app_names)
        except Exception as e:
            print(f"無法獲取應用程式列表: {e}")
            default_apps = ["Safari", "Google Chrome", "Microsoft Excel", "Visual Studio Code", "Terminal", "Notes"]
            return Response(default_apps)
