import serial
import numpy as np
import matplotlib.pyplot as plt
import time

# --- CONFIGURATION ---
SERIAL_PORT = 'COM12'    # <<< CHANGE THIS if your port changes!
BAUD_RATE = 500000       # <<< MUST MATCH the baud rate in the Arduino sketch!
IMAGE_WIDTH = 96
IMAGE_HEIGHT = 96
# ---------------------

def read_frame(ser):
    """Reads a single frame of image data from the serial port."""
    
    # Wait for the start marker 'I,'
    # Read up to 50 bytes until the start marker 'I,' is found
    line = ser.read_until(b'I,', size=50).decode('utf-8', errors='ignore')
    if not line.endswith('I,'):
        return None

    # Read the data until the end marker 'E'
    try:
        # *** FIX: Removed the unsupported 'timeout' keyword argument ***
        # The overall serial port timeout (0.5s) will now be used.
        data_line = ser.read_until(b'E').decode('utf-8', errors='ignore')
    except serial.SerialTimeoutException:
        print("Timeout reading frame data.")
        return None

    # Process the data (Remove the trailing 'E')
    if data_line.endswith('E'):
        data_line = data_line[:-1]
    
    # Split the comma-separated string into a list of integers
    try:
        # Filter out empty strings that can result from accidental commas
        pixel_values = [int(v.strip()) for v in data_line.split(',') if v.strip().isdigit()]
    except ValueError:
        return None
    
    if len(pixel_values) != IMAGE_WIDTH * IMAGE_HEIGHT:
        # Corrupted frame data
        return None
    
    # Convert to NumPy array and reshape to image dimensions
    image_array = np.array(pixel_values, dtype=np.uint8).reshape(IMAGE_HEIGHT, IMAGE_WIDTH)
    return image_array

def main():
    try:
        # Initialize serial port with a timeout of 0.5 seconds
        ser = serial.Serial(SERIAL_PORT, BAUD_RATE, timeout=0.5) 
        print(f"Connected to {SERIAL_PORT} at {BAUD_RATE} baud. Starting viewer...")
    except Exception as e:
        print(f"Error connecting to serial port: {e}")
        print("Please check SERIAL_PORT configuration and ensure the Arduino IDE Serial Monitor is CLOSED.")
        return

    # Setup the plot
    plt.ion() 
    fig, ax = plt.subplots()
    ax.set_title(f'Live Grayscale Feed ({IMAGE_WIDTH}x{IMAGE_HEIGHT})')
    
    # Initialize image display
    img = np.zeros((IMAGE_HEIGHT, IMAGE_WIDTH), dtype=np.uint8)
    image_display = ax.imshow(img, cmap='gray', vmin=0, vmax=255)
    
    while True:
        try:
            frame = read_frame(ser)
            
            if frame is not None:
                image_display.set_data(frame)
                
                # Force plot update
                fig.canvas.draw()
                fig.canvas.flush_events()
                
            time.sleep(0.01) 
            
        except KeyboardInterrupt:
            print("\nExiting viewer.")
            break
        except Exception as e:
            # Added a specific check for a common matplotlib error which can be ignored
            if "target is not alive" in str(e):
                 break
            print(f"An unexpected error occurred: {e}")
            time.sleep(1) 

    ser.close()
    plt.ioff()
    plt.close(fig)

if __name__ == "__main__":
    main()