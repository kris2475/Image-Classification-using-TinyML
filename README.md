🏭 An Ultra-Compact, Low-Cost Industrial Safety Solution

The true value of this system for industrial deployment lies in its minimal physical footprint, extremely low running cost, and autonomous operation. Designed for high-efficiency environments, the device spends nearly all of its lifespan in light sleep or deep sleep, drawing only microamps of current. It wakes only when needed, either on a precise Real-Time Clock (RTC) schedule or immediately via external triggers such as a PIR motion sensor.

This wake-on-demand strategy enables a self-sufficient, battery-friendly TinyML node capable of performing visual inspections without human supervision. When a potential Health & Safety (H&S) issue is detected—such as a fire door left open or a missing fire extinguisher—the system instantly raises an alert to the shop floor supervisor.

Instead of wasting time on low-value, repetitive monitoring, supervisors can redirect attention to higher-level operational duties, human management, and critical process optimisation. This is where AI delivers real industrial value: enhancing safety compliance, reducing oversight fatigue, and enabling leaner, more effective operations.

🚪 XIAO ESP32S3 TinyML Door Status Classifier

A real-time, ultra-low-power image classification system built with the Seeed Studio XIAO ESP32S3 Sense and TensorFlow Lite for Microcontrollers (TFLM).
Its goal: classify a door as Open or Closed—reliably, locally, and efficiently.

The project's success comes from a custom on-device data strategy designed to overcome the classic TinyML pitfalls of domain mismatch and quantization loss.

✨ Features & Project Strategy
Feature	Description
Edge AI Inference	Entire classification runs on the ESP32S3 MCU—low-latency, private, offline.
Data Fidelity	Model trained on images captured directly by the XIAO’s camera → domain-perfect training.
Robustness	Dataset includes lighting variations and environmental changes to avoid overfitting.
Optimized Input Pipeline	C++ preprocessing converts RGB565 camera frames to INT8 tensors efficiently.
PSRAM Utilization	A large 384 KB tensor arena is allocated in external PSRAM, preserving DRAM.
High Sensitivity	Detection threshold of 0.70 ensures reliable detection of the “Door Open” state.
🔬 Why Custom Data Was Essential
❌ The ImageNet Pitfall (Initial Attempt)

A model trained using ImageNet-like high-resolution images worked perfectly during FP32 training… but failed completely when deployed to the XIAO.

Why?
The XIAO’s OV2640 camera produces 96×96, low-color-depth, RGB565 images—nothing like ImageNet.
This created a domain mismatch, causing the model to misclassify nearly every frame.

✔️ The Custom Data Solution

A new dataset was created using the XIAO itself:

200 images (100 Open, 100 Closed)

Captured directly from the device’s camera

Environmental variability: lighting changes + furniture adjustments

Goal: teach structural differences, not shadows or backgrounds

This dataset produced a model that performs reliably under real-world embedded conditions.

💡 Final Classification Strategy (Critical Logic Reversal)

A key success factor for this project was the implementation of a final classification logic reversal to ensure reliability after camera tuning.

1. The Challenge of Low-Light Stability

With fixed manual exposure and gain settings (G=5, E=200), the model produced high “Closed” scores for truly closed-door scenes.
However, the “Open” score remained consistently low even when the door was open—making the typical logic:

“If Open Score > Threshold → Door is Open”

unreliable.

2. The Solution: Trust the Drop

The system instead monitors the drop in the reliable Closed Score:

If the Closed Score drops below the threshold, the door is considered OPEN.

If the Closed Score stays above the threshold, the door is CLOSED.

Door Status	Condition	Threshold
OPEN	Closed Score < 0.55	CLOSED_DOOR_THRESHOLD
CLOSED	Closed Score ≥ 0.55	CLOSED_DOOR_THRESHOLD
Final Logic Snippet
// FINAL LOGIC: Detect "OPEN" when the reliable "Closed" score DROPS below the threshold.
if (door_closed_score < CLOSED_DOOR_THRESHOLD) {
    // ... DOOR OPENED DETECTED!
} else {
    // ... Door Closed.
}

⚙️ Hardware & Software Requirements
Hardware

Seeed Studio XIAO ESP32S3 Sense (camera + PSRAM)

USB-C cable

Software / Dependencies

Arduino IDE or PlatformIO

ESP32 Board Support Package

TensorFlow Lite Micro libraries

⚠️ IDE Configuration (Critical)
Setting	Recommended Value	Notes
Board	XIAO ESP32S3	—
Partition Scheme	Huge App (3MB No OTA/FATFS)	Required for large TFLite model
PSRAM	OPI PSRAM	Needed for tensor arena
🚀 Getting Started
1. Model File (model.h)

This project requires a quantized TFLite model converted to a C array.

Steps:

Train an Open vs. Closed door classifier.

Quantize to INT8.

Convert the .tflite model into a C array (e.g., via xxd).

Place the array in model.h:

extern const unsigned char door_status_model[];


Place model.h in the project directory.

2. Upload the Sketch

Copy the project’s example .ino file

Confirm partition & PSRAM settings

Upload to XIAO ESP32S3 Sense

3. Viewing Classification Output

Open Serial Monitor @ 115200 baud.

Example:

--- XIAO ESP32S3 TinyML (Door Status Classifier) ---
Tensor Arena allocated successfully in PSRAM (393216 bytes).
Model loaded and ready.

DEBUG: Raw Scores: Closed=120, Open=115
🔒 Door Closed.
Highest 'Open' Score: 0.92 ('Closed' Score: 0.95)

DEBUG: Raw Scores: Closed=80, Open=127
🚪 DOOR OPENED DETECTED! Confidence: 0.99 (Threshold: 0.70)

💻 Key Preprocessing Logic

The project’s most critical function converts RGB565 frames → INT8 tensors.

// Conversion from RGB565 to INT8 (0–255 mapped to -128 to 127)
bool GetInputData() {
    // ... camera capture ...

    for (int i = 0; i < src_width * src_height; i++) {
        uint16_t pixel = src_buf[i * 2] | (src_buf[i * 2 + 1] << 8);

        // 1. Extract and expand R, G, B to 8-bit
        uint8_t r = ((pixel >> 11) & 0x1F) << 3;
        uint8_t g = ((pixel >> 5) & 0x3F) << 2;
        uint8_t b = (pixel & 0x1F) << 3;

        // 2. Normalize for INT8 model input
        dest_buf[dest_index++] = (int8_t)(r - 128);
        dest_buf[dest_index++] = (int8_t)(g - 128);
        dest_buf[dest_index++] = (int8_t)(b - 128);
    }

    // ... return frame buffer ...
}

🧠 Summary

This project demonstrates how TinyML models must be trained with hardware-matched data to perform effectively on microcontrollers.

By combining:

custom datasets

optimized preprocessing

efficient PSRAM usage

…the XIAO ESP32S3 becomes a robust, low-power door status detector ideal for industrial safety automation.
