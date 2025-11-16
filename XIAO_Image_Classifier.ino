/**
 * @file XIAO_Image_Classifier.ino
 * @brief FINAL SKETCH: Corrected to use 3-channel (RGB565) input to match the trained TFLite model,
 * and the detection threshold is lowered to 0.70 for increased sensitivity.
 * ---------------------------------------------------------------------------------
 * --- FIXES APPLIED ---
 * 1. RGB Input Channel Fix: Camera set to PIXFORMAT_RGB565 and GetInputData() correctly handles RGB565 to INT8.
 * 2. Sensitivity Fix: DETECTION_THRESHOLD lowered from 0.85 to 0.70.
 * ---------------------------------------------------------------------------------
 */

#include <Arduino.h>
#include <esp_heap_caps.h> 
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h> 
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_log.h>
#include <tensorflow/lite/micro/system_setup.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include "tensorflow/lite/micro/micro_utils.h" 
#include "esp_camera.h"
#include "esp_log.h" 
// You must ensure 'model.h' containing 'person_model' is in your project directory
#include "model.h" 

// --- CONSTANTS AND CONFIGURATION ---
const int kTensorArenaSize = 384 * 1024; 
#define PERSON_INDEX 1 
#define NO_PERSON_INDEX 0
#define DETECTION_THRESHOLD 0.70 // <-- ADJUSTED SENSITIVITY
#define MODEL_INPUT_WIDTH 96
#define MODEL_INPUT_HEIGHT 96

// --- GLOBAL VARIABLES ---
namespace {
  tflite::ErrorReporter* error_reporter = nullptr; 
  const tflite::Model* model = nullptr;
  tflite::MicroInterpreter* interpreter = nullptr; 
  TfLiteTensor* input = nullptr;
  TfLiteTensor* output = nullptr;
  uint8_t* tensor_arena = nullptr; 
}

// Camera configuration (Official Seeed Pinout and 20MHz XCLK)
static camera_config_t camera_config = {
    .pin_pwdn = -1,    // PWDN Pin disabled
    .pin_reset = -1, 
    
    // --- CORRECTED PINS (Official Seeed Pinout) ---
    .pin_xclk = 10, 
    .pin_sccb_sda = 40, 
    .pin_sccb_scl = 39, 
    
    // Data Pins (D7-D0)
    .pin_d7 = 48, .pin_d6 = 11, .pin_d5 = 12, .pin_d4 = 14, 
    .pin_d3 = 16, .pin_d2 = 18, .pin_d1 = 17, .pin_d0 = 15,
    
    // Sync Pins
    .pin_vsync = 38, .pin_href = 47, .pin_pclk = 13,
    .xclk_freq_hz = 20000000, // 20MHz for stability
    // --- CORRECTED PINS END ---

    .ledc_timer = LEDC_TIMER_0, .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_RGB565, // FIX: RGB565 for 3-channel model
    .frame_size = FRAMESIZE_96X96,    // FIX: Match model input size
    .jpeg_quality = 12,
    .fb_count = 1, 
    .fb_location = CAMERA_FB_IN_PSRAM, 
    .grab_mode = CAMERA_GRAB_LATEST
};


// --- CUSTOM ERROR REPORTER CLASS ---
class CustomErrorReporter : public tflite::ErrorReporter {
  public:
  int Report(const char* format, va_list args) override {
    vprintf(format, args);
    return 0;
  }
};


// --- CAMERA INITIALIZATION ---

void init_camera() {
    Serial.begin(115200);
    Serial.println("Initializing Camera...");
    if(psramFound()){
      Serial.println("PSRAM found. Using PSRAM for camera frame buffer.");
    } else {
      Serial.println("PSRAM NOT found. Camera stability might be affected.");
      camera_config.fb_location = CAMERA_FB_IN_DRAM;
    }
    
    if (esp_camera_init(&camera_config) != ESP_OK) {
        esp_err_t err_code = esp_camera_init(&camera_config);
        Serial.printf("!!! CRITICAL ERROR: Camera Init Failed (0x%x) !!!\n", err_code); 
        Serial.println("Please confirm IDE settings: OPI PSRAM, Huge App, 160MHz CPU.");
        Serial.println("Halting program to prevent continuous capture failures.");
        while(1) { delay(1000); } 
    }
    
    Serial.println("Camera initialized successfully.");

    // Disable Auto Controls (For high-accuracy/consistency in TinyML)
    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        s->set_gain_ctrl(s, 0); s->set_agc_gain(s, 0); s->set_exposure_ctrl(s, 0);
        s->set_whitebal(s, 0); s->set_awb_gain(s, 0);
        Serial.println("Camera settings locked: AGC/AEC/AWB are DISABLED.");
    }
}


// --- IMAGE ACQUISITION AND PRE-PROCESSING (RGB565 to INT8) ---

bool GetInputData() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) { Serial.println("Camera capture failed."); return false; }

    uint8_t *src_buf = fb->buf;
    uint16_t src_width = fb->width; // 96
    uint16_t src_height = fb->height; // 96
    int8_t *dest_buf = (int8_t *)input->data.int8; 
    
    size_t dest_index = 0; 
    
    // Iterate through all 96x96 pixels (2 bytes per pixel for RGB565)
    for (int i = 0; i < src_width * src_height; i++) {
        // Read 2 bytes as a 16-bit word
        uint16_t pixel = src_buf[i * 2] | (src_buf[i * 2 + 1] << 8); 

        // 1. Extract R, G, B components from RGB565 and expand to 8 bits (0-255)
        uint8_t r = ((pixel >> 11) & 0x1F) << 3; // 5 bits Red -> 8 bits
        uint8_t g = ((pixel >> 5) & 0x3F) << 2;  // 6 bits Green -> 8 bits
        uint8_t b = (pixel & 0x1F) << 3;         // 5 bits Blue -> 8 bits

        // 2. Quantization: Convert 0-255 (uint8_t) to -128 to 127 (int8_t) for each channel
        dest_buf[dest_index++] = (int8_t)(r - 128); // R channel
        dest_buf[dest_index++] = (int8_t)(g - 128); // G channel
        dest_buf[dest_index++] = (int8_t)(b - 128); // B channel
    }

    esp_camera_fb_return(fb);
    return true;
}


// --- TFLITE SETUP ---

void setup() {
    delay(1000); 

    init_camera();
    Serial.println("\n--- XIAO ESP32S3 TinyML (RGB Input Fixed) ---");

    tflite::InitializeTarget();
    
    static CustomErrorReporter micro_error_reporter; 
    error_reporter = &micro_error_reporter;
    
    model = tflite::GetModel(person_model);
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        MicroPrintf("Model version: %d is not supported by schema version: %d",
                    model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }
    
    // Manual Op Resolver: <11> OPS
    static tflite::MicroMutableOpResolver<11> resolver;
    resolver.AddConv2D();
    resolver.AddDepthwiseConv2D();
    resolver.AddFullyConnected();
    resolver.AddMaxPool2D();
    resolver.AddAveragePool2D();
    resolver.AddReshape();
    resolver.AddSoftmax();
    resolver.AddRelu();
    resolver.AddQuantize(); 
    resolver.AddDequantize();
    resolver.AddLogistic(); 

    // Allocate the tensor arena explicitly in PSRAM
    tensor_arena = (uint8_t*)heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM);
    if (tensor_arena == nullptr) {
        MicroPrintf("ERROR: Failed to allocate %d bytes for tensor arena in PSRAM!", kTensorArenaSize);
        return;
    }
    MicroPrintf("Tensor Arena allocated successfully in PSRAM (%d bytes).", kTensorArenaSize);


    // Use the long 7-argument constructor 
    static tflite::MicroInterpreter static_interpreter(
        model, 
        resolver, 
        tensor_arena, 
        (size_t)kTensorArenaSize, 
        nullptr, // MicroResourceVariables* resource_variables
        nullptr, // MicroProfilerInterface* profiler
        false    // bool enable_caching
    ); 
    
    interpreter = &static_interpreter;

    if (interpreter->AllocateTensors() != kTfLiteOk) {
        MicroPrintf("ERROR: AllocateTensors() failed! Total Arena Size: %d bytes", kTensorArenaSize);
        return;
    }
    
    input = interpreter->input(0);
    output = interpreter->output(0);
    
    Serial.println("Model loaded and ready.");
}


// --- INFERENCE AND REPORTING LOGIC ---

void RunInferenceAndReport() {
    // 1. Acquire Image and Pre-process (Resample/Quantize)
    if (!GetInputData()) return;

    // 2. Run Inference
    TfLiteStatus invoke_status = interpreter->Invoke();
    if (invoke_status != kTfLiteOk) {
        MicroPrintf("ERROR: Invoke() failed!");
        return;
    }

    // 3. De-Quantize Output Score (This logic is correct for an INT8 model output)
    int8_t person_score_quantized = output->data.int8[PERSON_INDEX];
    float person_score = (float)(person_score_quantized - output->params.zero_point) * output->params.scale;
    float no_person_score = (float)(output->data.int8[NO_PERSON_INDEX] - output->params.zero_point) * output->params.scale;
    
    // --- DEBUG PRINTS START (Keep these to verify score changes) ---
    Serial.printf("DEBUG: Scale=%.4f, ZeroPoint=%d\n", output->params.scale, output->params.zero_point);
    Serial.printf("DEBUG: Raw Scores: NoPerson=%d, Person=%d\n", 
                  output->data.int8[NO_PERSON_INDEX], 
                  output->data.int8[PERSON_INDEX]);
    // --- DEBUG PRINTS END ---

    // 4. Report Results
    // Check against the new, lower threshold of 0.70
    if (person_score >= DETECTION_THRESHOLD) {
        Serial.printf("✅ **DETECTION TRIGGERED!** Confidence: %.2f (Threshold: %.2f)\n", 
                      person_score, DETECTION_THRESHOLD);
    } else {
        Serial.printf("❌ No Detection. Highest Score: %.2f (No-Person: %.2f)\n", 
                      person_score, no_person_score);
    }
}


// --- MAIN LOOP ---

void loop() {
    RunInferenceAndReport();
    delay(1000); 
}
































