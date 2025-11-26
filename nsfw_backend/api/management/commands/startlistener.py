import time
import serial
import subprocess
import json
from django.core.management.base import BaseCommand
from django.core.cache import cache
from api.views import DEFAULT_SETTINGS

# --- Mac Control Functions ---
def set_brightness(level):
    """Sets screen brightness by simulating key presses. 0-16 steps."""
    # First, decrease to 0 to establish a baseline.
    decrease_command = ['osascript', '-e', 'tell application "System Events" to repeat 16 times', '-e', 'key code 145', '-e', 'end repeat']
    subprocess.run(decrease_command, check=False)
    
    # Then, increase to the target level.
    if level > 0:
        increase_command = ['osascript', '-e', f'tell application "System Events" to repeat {level} times', '-e', 'key code 144', '-e', 'end repeat']
        subprocess.run(increase_command, check=False)

def set_volume(level):
    """Sets system volume (0-100)."""
    subprocess.run(['osascript', '-e', f'set volume output volume {level}'], check=False)

def open_app(app_name):
    """Opens the specified application."""
    subprocess.run(['open', '-a', app_name], check=False)

class Command(BaseCommand):
    help = 'Starts the serial listener for the ultrasonic sensor'

    def handle(self, *args, **kwargs):
        self.stdout.write("Starting serial communication listener...")
        
        # --- Configuration ---
        serial_port = '/dev/cu.usbserial-BG02MH6B' 
        baud_rate = 9600
        
        # --- Sliding Window Filter Initialization ---
        HISTORY_SIZE = 10
        measurement_history = [0] * HISTORY_SIZE # 0:safe, 1:warning, 2:danger
        history_index = 0
        danger_count = 0
        warning_count = 0
        
        current_system_state = None
        
        # Track last sent config to avoid redundant serial writes
        last_sent_config = {}

        while True:
            try:
                self.stdout.write(f"Attempting to connect to serial port {serial_port}...")
                ser = serial.Serial(serial_port, baud_rate, timeout=2)
                self.stdout.write(self.style.SUCCESS("Serial port connected successfully!"))
                # Reset last sent config on new connection
                last_sent_config = {}

                while ser.is_open:
                    line = ser.readline().decode('utf-8').strip()
                    if not line: continue

                    # Get the latest settings from Redis, or use defaults.
                    settings_json = cache.get('app_settings')
                    settings = json.loads(settings_json) if settings_json else DEFAULT_SETTINGS

                    # --- Sync config with hardware if it has changed ---
                    if settings.get('warning_threshold') != last_sent_config.get('warning_threshold'):
                        warn_threshold = int(settings.get('warning_threshold', 100))
                        ser.write(f"W:{warn_threshold}\n".encode('utf-8'))
                        self.stdout.write(f"Sent to hardware: Set Warning Threshold -> {warn_threshold}cm")
                        last_sent_config['warning_threshold'] = settings.get('warning_threshold')

                    if settings.get('danger_threshold') != last_sent_config.get('danger_threshold'):
                        danger_threshold = int(settings.get('danger_threshold', 50))
                        ser.write(f"D:{danger_threshold}\n".encode('utf-8'))
                        self.stdout.write(f"Sent to hardware: Set Danger Threshold -> {danger_threshold}cm")
                        last_sent_config['danger_threshold'] = settings.get('danger_threshold')


                    # Parse distance and determine state
                    try:
                        # Parse "DIST:xxx" format
                        distance = int(line.split(":")[1])
                        self.stdout.write(f"Distance detected: {distance} cm")
                        
                        # Store distance in Redis for the StatusView to read
                        cache.set('current_distance', distance, timeout=5)

                        # --- Sliding Window Filter Logic ---
                        oldest_measurement = measurement_history[history_index]
                        if oldest_measurement == 2: danger_count -= 1
                        elif oldest_measurement == 1: warning_count -= 1

                        current_measurement_state = 0
                        if distance <= int(settings.get('danger_threshold', 50)):
                            current_measurement_state = 2
                            danger_count += 1
                        elif distance <= int(settings.get('warning_threshold', 100)):
                            current_measurement_state = 1
                            warning_count += 1
                        else:
                            current_measurement_state = 0
                        
                        measurement_history[history_index] = current_measurement_state
                        history_index = (history_index + 1) % HISTORY_SIZE

                        # --- Determine final state based on filtered result ---
                        final_state = 'safe'
                        if danger_count > (HISTORY_SIZE / 2):
                            final_state = 'danger'
                        elif warning_count > (HISTORY_SIZE / 2):
                            final_state = 'warning'

                        # Execute actions only if the final state has changed
                        if final_state != current_system_state:
                            current_system_state = final_state
                            self.stdout.write(self.style.SUCCESS(f"State changed -> {current_system_state.upper()}"))
                            
                            config = settings[current_system_state]
                            set_volume(int(config['volume']))
                            set_brightness(int(config['brightness']))
                            if current_system_state == 'danger':
                                open_app(config['target_app'])

                    except (IndexError, ValueError):
                        continue # Ignore malformed lines

            except serial.SerialException:
                self.stdout.write(self.style.ERROR(f"Failed to connect to serial port. Retrying in 5 seconds..."))
                time.sleep(5)
            except KeyboardInterrupt:
                self.stdout.write("Listener stopped by user.")
                break
