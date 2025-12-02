import subprocess
import os
import json
from rest_framework.views import APIView
from rest_framework.response import Response
from rest_framework import status
from django.core.cache import cache

# Default settings, will be overridden by cache if available
DEFAULT_SETTINGS = {
    "safe": {"volume": 80, "brightness": 16},
    "warning": {"volume": 40, "brightness": 8},
    "danger": {"volume": 0, "brightness": 0, "target_app": "Microsoft Excel"},
    "warning_threshold": 100,
    "danger_threshold": 50,
    "led_alerts_enabled": True,    # New default state for LED alerts
    "buzzer_alerts_enabled": True, # New default state for Buzzer alerts
}

class SettingsView(APIView):
    """
    讀取和更新三個狀態的設定
    """
    def get(self, request, format=None):
        settings_json = cache.get('app_settings')
        if settings_json:
            return Response(json.loads(settings_json))
        return Response(DEFAULT_SETTINGS)

    def post(self, request, format=None):
        data = request.data
        # Store the entire settings object as a JSON string in Redis
        cache.set('app_settings', json.dumps(data), timeout=None) # timeout=None for persistence
        print(f"設定已更新並存入 Redis: {data}")
        return Response(data, status=status.HTTP_200_OK)

class HardwareControlView(APIView):
    """
    控制硬體 (LED/Buzzer)
    """
    def post(self, request, device, format=None):
        command = None
        if device == 'led':
            command = 'TOGGLE_LED'
        elif device == 'buzzer':
            command = 'TOGGLE_BUZZER'
        
        if command:
            cache.client.get_client().publish('hardware-commands', command)
            print(f"Published {command} command via Redis Pub/Sub.")
        return Response({"status": f"Toggle command for {device} sent."})

class StatusView(APIView):
    """
    獲取目前感測器狀態
    """
    def get(self, request, format=None):
        # Read the latest distance from Redis cache
        distance = cache.get('current_distance')
        if distance is not None:
            return Response({"distance": int(distance)})
        return Response({"distance": "N/A"})

class ApplicationsView(APIView):
    """
    獲取 macOS 上的應用程式列表
    """
    def get(self, request, format=None):
        default_apps = ["Safari", "Google Chrome", "Microsoft Excel", "Visual Studio Code", "Terminal", "Notes"]
        try:
            command = ['mdfind', "kMDItemKind == 'Application'"]
            result = subprocess.run(command, capture_output=True, text=True, check=True, encoding='utf-8')
            paths = result.stdout.strip().split('\n')
            # Use a set to avoid duplicates, then merge with default apps
            scanned_apps = set(os.path.basename(p)[:-4] for p in paths if p.endswith('.app') and os.path.basename(p)[:-4])
            combined_apps = sorted(list(scanned_apps.union(set(default_apps))))
            return Response(combined_apps)
        except Exception as e:
            print(f"無法獲取應用程式列表: {e}")
            # If mdfind fails entirely, just return the default list
            return Response(default_apps)
