🚪 XIAO ESP32S3 TinyML Door Status Classifier

This project features a real-time, ultra-low-power image classification system using the Seeed Studio XIAO ESP32S3 Sense and TensorFlow Lite for Microcontrollers (TinyML). Its sole purpose is to classify the state of a door as either Open or Closed with high reliability.

The core success of this deployment lies in its custom data strategy, built specifically to overcome the common challenges of domain mismatch and quantization loss in edge AI projects.

✨ Features & Project Strategy

Feature

Description

Edge AI Inference

Classification runs entirely on the ESP32S3 MCU, ensuring low latency and maximum privacy.

Data Fidelity

The model is trained on data captured directly by the XIAO's camera, ensuring the training domain matches the deployment domain.

Robustness

Dataset included variations in object placement (furniture) and natural/artificial lighting to prevent overfitting to environmental factors.

Optimized Input Pipeline

Efficient C++ logic handles the critical conversion and quantization from the camera's low-depth RGB565 format to the model's required INT8 input.

PSRAM Utilization

Uses the external PSRAM (SPIRAM) for the large $384 \text{ KB}$ tensor arena, saving internal DRAM for system processes.

High Sensitivity

Detection threshold set at 0.70 for reliable positive detection of the critical "Door Open" state.

🔬 Project Context: Why Custom Data is Key

This project's final design was a direct result of overcoming deployment challenges:

The ImageNet Pitfall (Initial Attempt)

An initial model trained on a large, generic dataset like ImageNet performed extremely well in the Google Colab training environment (FP32 precision) but failed spectacularly upon deployment to the XIAO.

The Problem: The high-resolution, high-fidelity images of ImageNet represented a completely different domain than the $96 \times 96$ low-color-depth images captured by the XIAO's fixed-lens OV2640 camera.

The Result: The model was unable to correctly classify the low-fidelity, noisy images produced by the target hardware, a classic Domain Mismatch failure.

The Custom Data Solution

To fix this, a new dataset was created on-site:

Direct Capture: 200 images (100 'Door Closed', 100 'Door Open') were captured using the XIAO's camera itself.

Forced Variability: During capture, furniture was moved and natural and artificial lighting conditions were intentionally altered to ensure the model learned to classify the structural difference between open/closed, rather than just the background shadows or objects.

This custom, high-variability dataset was essential to train a model that works reliably under real-world, embedded conditions.

⚙️ Hardware & Software Requirements

Hardware

Seeed Studio XIAO ESP32S3 Sense (with integrated camera and PSRAM)

Micro USB-C Cable

Software / Dependencies

Arduino IDE or VS Code (PlatformIO)

ESP32 Board Support Package

TensorFlow Lite Micro libraries

⚠️ IDE Configuration (Crucial for Memory)

To allocate the necessary PSRAM and program the large model file, ensure these settings are selected in your IDE:

Setting

Recommended Value

Notes

Board

XIAO ESP32S3



Partition Scheme

Huge App (3MB No OTA/FATFS)

Required for large TFLite model and application code.

PSRAM

OPI PSRAM

Enables the use of external PSRAM for the tensor arena.

🚀 Getting Started

1. Model File (model.h)

This sketch relies on a pre-trained, quantized TFLite model.

Train your Door Closed vs. Door Open classification model.

Quantize the model to INT8.

Convert the quantized model to a C array format (e.g., using xxd or TFLM tools).

Ensure the C array is defined as door_status_model and saved within a file named model.h.

Place model.h in the project directory alongside the main sketch.

2. Upload the Sketch

Copy the contents of the provided Arduino sketch (XIAO_Image_Classifier_Door.ino).

Verify your IDE settings (Partition Scheme, PSRAM) match the table above.

Upload the sketch to the XIAO ESP32S3 Sense board.

3. Monitoring Results

Open the Serial Monitor at 115200 baud. The device will immediately begin classifying the camera feed every 1 second.

Example Output:

--- XIAO ESP32S3 TinyML (Door Status Classifier) ---
...
Tensor Arena allocated successfully in PSRAM (393216 bytes).
Model loaded and ready.
DEBUG: Raw Scores: Closed=120, Open=115
🔒 Door Closed. Highest 'Open' Score: 0.92 ('Closed' Score: 0.95)
...
DEBUG: Raw Scores: Closed=80, Open=127
🚪 **DOOR OPENED DETECTED!** Confidence: 0.99 (Threshold: 0.70)


💻 Key Pre-Processing Logic

The GetInputData() function performs the crucial conversion of the raw camera data (PIXFORMAT_RGB565, a 16-bit color format) into the 8-bit, normalized integers (INT8) expected by the TFLite model.

// Conversion from RGB565 to INT8 (0-255 mapped to -128 to 127)
bool GetInputData() {
    // ... camera capture ...
    for (int i = 0; i < src_width * src_height; i++) {
        uint16_t pixel = src_buf[i * 2] | (src_buf[i * 2 + 1] << 8); 

        // 1. Extract and Expand R, G, B components to 8 bits (0-255)
        uint8_t r = ((pixel >> 11) & 0x1F) << 3; 
        uint8_t g = ((pixel >> 5) & 0x3F) << 2; 
        uint8_t b = (pixel & 0x1F) << 3; 

        // 2. Quantization (Normalization): Convert 0-255 to -128 to 127
        dest_buf[dest_index++] = (int8_t)(r - 128); // R channel
        dest_buf[dest_index++] = (int8_t)(g - 128); // G channel
        dest_buf[dest_index++] = (int8_t)(b - 128); // B channel
    }
    // ... return frame buffer ...
}
