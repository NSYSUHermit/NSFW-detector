# Dual Ultrasonic Privacy Guardian

<p align="center">
  <img src="https://brandguide.asu.edu/assets/img/asu-logos/asu_logo_maroon_rgb.png" alt="Arizona State University Logo" width="200"/>
</p>

An advanced, highly configurable privacy shield for your workspace. Using two ultrasonic sensors, this system detects proximity and automatically adjusts your Mac's state—including volume, screen brightness, and the active application—based on rules you define in a sleek, ASU-themed web interface.

## Features

*   **Dual Sensor Detection**: Utilizes two HC-SR04 ultrasonic sensors for wider and more reliable proximity detection.
*   **Three-Stage Alert System**: Configure distinct actions for "Safe," "Warning," and "Danger" zones.
*   **Real-time Remote Configuration**: A web-based UI allows you to dynamically adjust distance thresholds, volume, brightness, and the target "panic" application.
*   **Live Hardware Sync**: Settings saved in the web UI are instantly sent to the microcontroller, updating its alert behavior in real-time.
*   **Advanced Stability Control**: Implements a sliding window filter on both the hardware and backend to prevent false triggers from erratic readings.
*   **ASU-Themed Interface**: A polished and professional user interface featuring the official Arizona State University logo and color palette.

## System Architecture

The system consists of three main components that communicate seamlessly: the **Hardware** (AVR microcontroller), the **Backend** (Django/Redis), and the **Frontend** (React).

```
+-----------------+   USB Serial   +------------------------------------------------+
|                 | <------------> |  Backend (Django)                              |
|   Hardware      |                |  +------------------+   +--------------------+ |
| (AVR Micro)     |                |  | startlistener.py |   | runserver (views.py) | |
|                 |                |  +------------------+   +--------------------+ |
+-----------------+                |          ^  |  ^                |  ^             |
| - Dual HC-SR04  |                |          |  v  |                v  |             |
| - RGB LED       |                |          +----+--------------------+             |
| - Buzzer/Button |                |               | Redis Cache        |             |
| - C Firmware    |                |               +--------------------+             |
+-----------------+                |                          ^  |                  |
                                   |                          |  v   HTTP/API       |
                                   |  +--------------------------------------------+
                                   |  | Frontend (React)                           |
                                   |  +--------------------------------------------+
                                   |  | - Configuration UI                         |
                                   |  | - Real-time Status Display                 |
                                   |  +--------------------------------------------+
```

1.  **Hardware**: The microcontroller continuously measures distance and sends the minimum value to the backend via USB serial. It also listens for configuration commands from the backend.
2.  **Backend Listener (`startlistener.py`)**: A persistent service that reads data from the serial port, applies a filter, writes the distance to Redis, and executes Mac control commands based on rules read from Redis. It also sends configuration updates to the hardware.
3.  **Backend Web Server (`runserver`)**: A Django server that serves the frontend and provides an API. It reads distance from Redis to show on the UI and writes user settings from the UI into Redis.
4.  **Frontend**: A React single-page application that provides the user interface for configuration and status monitoring.

## Technology Stack

*   **Hardware**: C, `avr-gcc`, `avrdude`
*   **Backend**: Python, Django, Django REST Framework, Redis, `django-redis`, `pyserial`
*   **Frontend**: JavaScript, React, HTML/CSS
*   **System Control**: `osascript` (AppleScript)

## Setup and Installation

### 1. Hardware Setup

**Components:**
*   AVR Microcontroller (e.g., Arduino Uno/Nano)
*   2 x HC-SR04 Ultrasonic Sensors
*   1 x Common Cathode RGB LED
*   1 x Buzzer
*   1 x Push Button
*   Breadboard and jumper wires

**Wiring Guide:**
Connect the components to your microcontroller according to the pin definitions in `finals.c`:

| Component             | Pin on Board |
| --------------------- | ------------ |
| **Sensor 1 Trigger**  | `PB0`        |
| **Sensor 1 Echo**     | `PD2` (INT0) |
| **Sensor 2 Trigger**  | `PC0`        |
| **Sensor 2 Echo**     | `PD3` (INT1) |
| **RGB LED - Red**     | `PB1`        |
| **RGB LED - Green**   | `PD6`        |
| **RGB LED - Blue**    | `PD5`        |
| **Buzzer**            | `PB2`        |
| **Button**            | `PC4`        |

### 2. Software Prerequisites

Ensure you have the following installed on your Mac:
*   **Homebrew**: For installing packages.
*   **Python & Pip**: For the backend.
*   **Node.js & npm**: For the frontend.
*   **Redis**: `brew install redis`
*   **AVR Toolchain**: `brew install avr-gcc avrdude`

### 3. Backend Setup

```bash
# 1. Navigate to the backend directory
cd /path/to/your/project/NSFW-detector/nsfw_backend/

# 2. (Recommended) Create and activate a virtual environment
python3 -m venv venv
source venv/bin/activate

# 3. Install Python dependencies
pip install django djangorestframework pyserial django-redis redis

# 4. Initialize the Django database (for built-in apps)
python manage.py migrate
```

### 4. Firmware Setup

```bash
# 1. Navigate to the project's root directory
cd /path/to/your/project/NSFW-detector/

# 2. (If needed) Edit the Makefile to match your programmer and port
# By default, it's set for an Arduino Uno.

# 3. Compile and flash the firmware to your microcontroller
make flash
```

### 5. Frontend Setup

```bash
# 1. Navigate to the frontend directory
cd /path/to/your/project/NSFW-detector/nsfw_frontend/

# 2. Install Node.js dependencies
npm install
```

## Running the Application

You must run **three separate processes** in three different terminal windows.

### Terminal 1: Start the Redis Server

Make sure the Redis service is running.
```bash
# Check status
brew services list

# If not started, run:
brew services start redis
```

### Terminal 2: Start the Backend Listener

This process connects to the hardware and executes commands.
```bash
cd /path/to/your/project/NSFW-detector/nsfw_backend/
# Make sure your virtual environment is activated if you created one
# source venv/bin/activate 
python manage.py startlistener
```
You should see messages indicating it's trying to connect to the serial port.

### Terminal 3: Start the Backend Web Server

This process serves the API and the frontend.
```bash
cd /path/to/your/project/NSFW-detector/nsfw_backend/
# Make sure your virtual environment is activated
# source venv/bin/activate
python manage.py runserver
```

### Terminal 4: Start the Frontend

This process starts the React development server and opens the UI in your browser.
```bash
cd /path/to/your/project/NSFW-detector/nsfw_frontend/
npm start
```
Your default browser should open to `http://localhost:3000`, displaying the control interface.

## How to Use the Web Interface

The UI is divided into several panels:

*   **Status Bar (Top)**: The header displays the real-time distance detected by the sensors and a summary of your currently **saved and active** configuration for each state.
*   **Safe State Panel**: Configure the default volume and brightness for when no one is near.
*   **Warning State Panel**: Set the distance threshold (in cm) that triggers the "Warning" state, along with the desired volume and brightness.
*   **Danger State Panel**: Set the distance threshold for the "Danger" state, the corresponding volume/brightness, and the specific application to switch to.
*   **Save All Settings**: After making changes, click this button to save your configuration. The new rules will be sent to the backend and synced with the hardware in real-time.

## Troubleshooting

*   **Frontend shows "N/A" for distance**:
    1.  Check the **`startlistener`** terminal. Is it running and successfully printing `Distance detected: ...`?
    2.  If not, ensure no other program (`screen`, `cat`, another script) is using the serial port.
    3.  Check that the Redis service is running (`brew services list`).

*   **`Error: That port is already in use` when running `runserver`**:
    An old `runserver` process is likely stuck. Find its PID with `lsof -i :8000` and terminate it with `kill -9 <PID>`.

*   **`Unknown command: 'startlistener'`**:
    The directory structure for the command is incorrect. Ensure the path is exactly `api/management/commands/startlistener.py`, and that the `management` and `commands` directories both contain an empty `__init__.py` file.