🏭 Ultra-Compact, Low-Cost Industrial Safety System

Autonomous TinyML Door Status Classifier Using XIAO ESP32S3 Sense

Overview

This project demonstrates how an ultra-low-power, battery-friendly TinyML node can deliver real industrial safety value with minimal hardware cost and an exceptionally small footprint.
The device spends nearly all of its lifetime in light sleep or deep sleep, waking only via:

a Real-Time Clock (RTC) schedule, or

external triggers such as a PIR motion sensor.

This wake-on-demand approach allows the system to operate autonomously for long periods while performing local, private, on-device visual inspection.

When a potential Health & Safety (H&S) issue is detected—such as an open fire door or a missing extinguisher—the system immediately notifies a supervisor.
This shifts operator effort away from repetitive monitoring and toward higher-value operational tasks, improving safety compliance and reducing oversight fatigue.

🚪 XIAO ESP32S3 TinyML Door Status Classifier

A real-time, ultra-low-power image classification system built using:

Seeed Studio XIAO ESP32S3 Sense (with camera + PSRAM)

TensorFlow Lite for Microcontrollers (TFLM)

Its purpose: classify a door as Open or Closed—reliably and efficiently—directly on the MCU.

The model achieves robust performance thanks to a custom device-captured dataset, avoiding TinyML pitfalls like domain mismatch and quantization degradation.

✨ Features & Project Strategy
Feature	Description
Edge AI Inference	Full classification runs locally on the ESP32S3—private, offline, low-latency.
Data Fidelity	Training images captured directly from the XIAO's camera → domain-perfect training.
Robustness	Dataset includes lighting and environmental variations to prevent overfitting.
Optimized Input Pipeline	C++ preprocessing converts RGB565 → INT8 tensors efficiently.
PSRAM Utilization	Large 384 KB tensor arena stored in PSRAM to preserve DRAM.
High Sensitivity	Detection threshold of 0.70 for reliable Open-door classification.
🔬 Why Custom Data Was Essential
❌ The ImageNet Pitfall

Initial experiments using ImageNet-like high-res images failed:

OV2640 camera outputs 96×96 RGB565, a tiny and low-color-depth format.

FP32 models trained on high-quality images did not transfer to TFLM at all.

Result: nearly every frame was misclassified.

✔️ The Custom Data Solution

A new dataset was built using the actual device hardware:

200 images (100 Open / 100 Closed)

Captured directly with the XIAO ESP32S3 Sense

Environmental variation: lighting + furniture adjustments

Focused on structural cues, not backgrounds

This produced a model that performs reliably under real-world embedded conditions.

💡 Final Classification Strategy (Critical Logic Reversal)

A crucial reliability improvement came from applying a logic reversal after camera tuning.

1. The Challenge: Low-Light Stability

Manual exposure + gain (G=5, E=200) resulted in:

High “Closed” scores for closed scenes

Low “Open” scores even when door was open → unreliable

So the typical logic:

If OpenScore > Threshold → Door is Open


did not work.

2. The Solution: Trust the Most Stable Class

The Closed score was extremely stable and consistent.

So instead we detect the drop in Closed confidence:

Door Status	Condition	Threshold
OPEN	ClosedScore < 0.55	CLOSED_DOOR_THRESHOLD
CLOSED	ClosedScore ≥ 0.55	CLOSED_DOOR_THRESHOLD
Final Logic Snippet
// FINAL LOGIC: Detect "OPEN" when the reliable "Closed" score DROPS below the threshold.
if (door_closed_score < CLOSED_DOOR_THRESHOLD) {
    // ... DOOR OPENED DETECTED!
} else {
    // ... Door Closed.
}

✅ Notes on Classification Logic Reversal
(Why This Is Perfectly Valid in ML)

The logic-reversal strategy is fully legitimate and widely used in ML systems—especially in TinyML and embedded vision.

Real-world ML often requires:

trusting the most stable class score

detecting drops instead of peaks

applying domain-specific post-processing

shifting thresholds for sensor-dependent reliability

This approach is standard in:

anomaly detection

one-class classification

threshold-based event detection

safety-critical ML pipelines

The ML model still performs the core classification; the application interprets its outputs in the most stable and environment-appropriate way.

This is not a workaround—it is good engineering.

⚙️ Hardware & Software Requirements
Hardware

Seeed Studio XIAO ESP32S3 Sense

USB-C cable

Software

Arduino IDE or PlatformIO

ESP32 board package

TensorFlow Lite for Microcontrollers

⚠️ Critical IDE Configuration
Setting	Value	Notes
Board	XIAO ESP32S3	—
Partition Scheme	Huge App (3MB No OTA/FATFS)	Required for ~300–500 KB model
PSRAM	OPI PSRAM	Needed for tensor arena
🚀 Getting Started
1. Model File (model.h)

Steps:

Train an Open/Closed classifier

Quantize to INT8

Convert .tflite → C array (xxd -i)

Place into model.h:

extern const unsigned char door_status_model[];


Place this file in your project folder.

2. Upload the Sketch

Load the .ino example

Confirm Huge App + PSRAM

Upload to XIAO ESP32S3 Sense

3. View Classification Output

Serial Monitor → 115200 baud

Example Output
--- XIAO ESP32S3 TinyML (Door Status Classifier) ---
Tensor Arena allocated successfully in PSRAM (393216 bytes).
Model loaded and ready.

DEBUG: Raw Scores: Closed=120, Open=115
🔒 Door Closed.

DEBUG: Raw Scores: Closed=80, Open=127
🚪 DOOR OPENED DETECTED! Confidence: 0.99

💻 Key Preprocessing Logic
(RGB565 → INT8 Tensor Conversion)
// Conversion from RGB565 to INT8 (0–255 mapped to -128 to 127)
bool GetInputData() {
    // ... camera capture ...

    for (int i = 0; i < src_width * src_height; i++) {
        uint16_t pixel = src_buf[i * 2] | (src_buf[i * 2 + 1] << 8);

        uint8_t r = ((pixel >> 11) & 0x1F) << 3;
        uint8_t g = ((pixel >> 5) & 0x3F) << 2;
        uint8_t b = (pixel & 0x1F) << 3;

        dest_buf[dest_index++] = (int8_t)(r - 128);
        dest_buf[dest_index++] = (int8_t)(g - 128);
        dest_buf[dest_index++] = (int8_t)(b - 128);
    }

    // ... return frame buffer ...
}

🧠 Summary

This project illustrates the importance of:

hardware-matched training data

optimized preprocessing

careful PSRAM memory planning

domain-informed classification logic

The result is a tiny, efficient, industrial-ready door status detector ideal for:

safety automation

compliance monitoring

low-cost industrial IoT

battery-powered inspection nodes

🚪 Logic Reversal in the Door Status ExampleIn the image, the system is classifying the state of a door (Open or Closed) based on a confidence score, called the ClosedScore.1. Standard Logic (Implicit):A system might naturally be trained to output a high score for its primary class, say CLOSED.CLOSED when ClosedScore $\geq 0.55$OPEN when ClosedScore $< 0.55$2. Logic Reversal Strategy (Explicit):The image describes the same logic, but structured with an intentional reversal in how the conditions are written, or how the conditions are prioritized in the code:ConditionThresholdResultOPENClosedScore < 0.55The score is low, which means the door is NOT closed, so it must be OPEN.CLOSEDClosedScore $\geq 0.55$The score is high, which means the door is CLOSED.The "reversal" here isn't a complex philosophical concept like the "Reversal Curse" (which deals with a model's inability to learn symmetric facts). Instead, it's a programming or implementation pattern where:The system uses the ClosedScore to make a decision about OPEN.The condition for OPEN is the inverse (the reversal) of the primary CLOSED condition.3. Why Logic Reversal is Used in TinyMLThis strategy is particularly valuable in TinyML (Machine Learning on microcontrollers) and embedded vision due to resource constraints:Code Optimization: By focusing the decision logic on a single primary condition (ClosedScore $\geq 0.55$) and making the other output the default inverse (else or !), developers can often write simpler, more compact code.Reduced Complexity: A common scenario is to check only the failure condition first. For example, the code might be structured to simply check: if (ClosedScore < CLOSED_DOOR_THRESHOLD) then the output is OPEN, and then the code doesn't need to explicitly check the high score condition; it just defaults to CLOSED for everything else. This reduces the number of comparisons or branches the tiny processor has to execute.In this context, the logic-reversal strategy is a clever, legitimate engineering tactic to achieve efficiency by exploiting the binary nature of the classification output.
