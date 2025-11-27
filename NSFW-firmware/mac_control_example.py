import subprocess
import time
import os

def increase_brightness(up_index=1):
  """
  """
  command = [
      'osascript',
      '-e', 'tell application "System Events"',
      '-e', 'key code 144',
      '-e', 'end tell'
  ]
  for _ in range(up_index):
    subprocess.run(command)

def decrease_brightness(down_index=1):
  """
  """
  command = [
      'osascript',
      '-e', 'tell application "System Events"',
      '-e', 'key code 145',
      '-e', 'end tell'
  ]
  for _ in range(down_index):
    subprocess.run(command)

def set_volume(volume_level):
    """
    Sets the output volume to a specific level (0-100).
    """
    command = [
        'osascript',
        '-e', f'set volume output volume {volume_level}'
    ]
    subprocess.run(command)

def open_app(app_name):
    """Opens the specified application."""
    command = ['open', '-a', app_name]
    subprocess.run(command)

if __name__ == '__main__':
  set_volume(0)

  for _ in range(3):
    decrease_brightness(2)
    time.sleep(0.1)

  time.sleep(2)

  for _ in range(3):
    increase_brightness(2)
    time.sleep(0.1)

  open_app("Safari")
