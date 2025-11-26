import serial
import time

def main():
    """
    A stable example to read and print distance values from a serial port.
    This script is independent of Django or any web framework.
    """
    # --- CONFIGURATION ---
    # !!! IMPORTANT: Replace with your actual serial port name !!!
    # You can find it by running `ls /dev/cu.*` in your terminal.
    SERIAL_PORT = '/dev/cu.usbserial-BG02MH6B'
    BAUD_RATE = 9600
    # ---------------------

    print("--- Serial Reader Example ---")
    print(f"Attempting to connect to {SERIAL_PORT} at {BAUD_RATE} bps.")
    print("Press Ctrl+C to exit.")

    while True:
        try:
            # Establish a serial connection. The `with` statement ensures it's closed properly.
            with serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=1) as ser:
                print(f"Successfully connected to {SERIAL_PORT}. Waiting for data...")
                
                while True:
                    # Read one line from the serial port
                    line = ser.readline().decode('utf-8').strip()

                    # If the line is not empty, process it
                    if line:
                        # Your `finals.c` outputs "DIST:xxx"
                        if line.startswith("DIST:"):
                            try:
                                # Extract the number after "DIST:"
                                distance = int(line.split(':')[1])
                                print(f"Distance received: {distance} cm")
                            except (ValueError, IndexError):
                                print(f"Could not parse line: {line}")
                        else:
                            # Print any other lines received (e.g., startup messages)
                            print(f"INFO: {line}")

        except serial.SerialException:
            print(f"Failed to connect to {SERIAL_PORT}. Retrying in 5 seconds...")
            time.sleep(5)
        except KeyboardInterrupt:
            print("\nExiting program.")
            break

if __name__ == '__main__':
    main()
