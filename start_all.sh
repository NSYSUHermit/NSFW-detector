#!/bin/bash

# ==============================================================================
#   Dual Ultrasonic Privacy Guardian - Start All Services Script
# ==============================================================================
# This script automates the startup of all necessary components:
#   1. Backend Django Web Server (runserver)
#   2. Backend Hardware Listener (startlistener)
#   3. Frontend React Development Server (npm start)
#
# Usage:
#   - Make sure this script is executable: chmod +x start_all.sh
#   - Run it from the project root directory: ./start_all.sh
#   - Press Ctrl+C in the terminal to stop all services gracefully.
# ==============================================================================

echo "--- Starting All Services for Dual Ultrasonic Privacy Guardian ---"

# --- VIRTUAL ENVIRONMENT ACTIVATION ---
# This is the most critical part. We must use the Python from your Conda env.
# Replace this path if your conda installation is located elsewhere.
PYTHON_EXEC="/opt/anaconda3/envs/torch/bin/python"

# Define project paths for clarity
BASE_DIR=$(pwd)
BACKEND_DIR="$BASE_DIR/nsfw_backend"
FRONTEND_DIR="$BASE_DIR/nsfw_frontend"

# Function to clean up background processes on exit
cleanup() {
    echo -e "\n--- Shutting down all services... ---"
    # This command kills all processes in the current process group, which includes
    # all the background jobs started by this script.
    kill 0
    echo "All services stopped."
}

# Trap the EXIT signal (when the script is closed) and the INT signal (Ctrl+C)
# to run the cleanup function.
trap cleanup EXIT INT

# Start Backend Services
echo "[1/3] Starting Backend Django services (runserver & startlistener)..."
# We explicitly use the Python executable from your Conda environment.
# The commands are grouped to ensure they both run in the correct directory.
(cd "$BACKEND_DIR" && { $PYTHON_EXEC manage.py runserver & $PYTHON_EXEC manage.py startlistener; }) &

# Start Frontend Service
echo "[2/3] Starting Frontend React server..."
(cd "$FRONTEND_DIR" && npm start) &

echo "[3/3] All services are starting up in the background."
echo "--- Press Ctrl+C to stop all services. ---"

# The 'wait' command keeps the script alive, waiting for all background jobs.
# This allows the 'trap' to work correctly when you press Ctrl+C.
wait