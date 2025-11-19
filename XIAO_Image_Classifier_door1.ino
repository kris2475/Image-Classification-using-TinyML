/**
 * @file XIAO_Image_Classifier_Door.ino
 * @brief FINAL DEFINITIVE SKETCH: Stable Door Status Classifier for XIAO ESP32S3 Sense.
 * * This sketch resolves multiple critical issues found during development:
 * * ## 1. Memory and Initialization Stability
 * - **Issue:** The large TFLite model required explicit allocation in **PSRAM** (kTensorArenaSize = 384 KB) 
 * using `heap_caps_malloc(..., MALLOC_CAP_SPIRAM)` to prevent hard faults and crashes.
 * - **Fix:** Confirmed PSRAM use for both the Tensor Arena and the Camera Frame Buffer (`fb_location = CAMERA_FB_IN_PSRAM`).
 * * ## 2. Camera Exposure and Classification Drift
 * - **Issue:** The default Auto Exposure/Gain (AEC/AGC) caused high noise and fluctuating scores, leading to unreliable classification, especially in low-light (closed door) conditions.
 * - **Fix:** Switched to **Fixed Manual Exposure** (`s->set_gain_ctrl(s, 0)`), aggressively tuning the settings 
 * (**Gain=5, Exposure=200**) to ensure the dark, closed-door scene is stable and correctly differentiated from the open scene.
 * * ## 3. Final Classification Logic
 * - **Issue:** After camera tuning, the model correctly generated a high score for "Closed" when the door was closed, 
 * but the "Open" score remained reliably low, even when the door was open, making the standard "Open Score > Threshold" logic fail.
 * - **Fix (Logic Reversal):** Switched the detection logic to trigger on the most reliable signal: the **drop in the Closed Score**. 
 * The door is classified as **OPEN** if the **Closed Score drops below 0.55** (CLOSED_DOOR_THRESHOLD).
 * * @note This final configuration ensures high stability and accurate classification for the door status application.
 */

#include <Arduino.h>
#include <esp_heap_caps.h> 
// TFLite Includes
#include <tensorflow/lite/micro/micro_mutable_op_resolver.h> 
#include <tensorflow/lite/micro/micro_interpreter.h>
#include <tensorflow/lite/micro/micro_log.h>
#include <tensorflow/lite/micro/system_setup.h>
#include <tensorflow/lite/schema/schema_generated.h>
#include "tensorflow/lite/micro/micro_utils.h" 
#include "esp_camera.h"
#include "esp_log.h" 
#include "door_status_model.h" 

// --- CONSTANTS AND CONFIGURATION ---
// STANDARD LOGIC: Index 0 is Closed, Index 1 is Open 
#define DOOR_OPEN_INDEX 1 
#define DOOR_CLOSED_INDEX 0
const int kTensorArenaSize = 384 * 1024; // 384KB in PSRAM
#define CLOSED_DOOR_THRESHOLD 0.55 // FINAL LOGIC: Door is OPEN if Closed Score drops below 0.55
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

// Camera configuration (XIAO ESP32S3 Sense Pinout)
static camera_config_t camera_config = {
    .pin_pwdn = -1, .pin_reset = -1, .pin_xclk = 10, .pin_sccb_sda = 40, .pin_sccb_scl = 39, 
    .pin_d7 = 48, .pin_d6 = 11, .pin_d5 = 12, .pin_d4 = 14, .pin_d3 = 16, .pin_d2 = 18, .pin_d1 = 17, .pin_d0 = 15,
    .pin_vsync = 38, .pin_href = 47, .pin_pclk = 13,
    .xclk_freq_hz = 20000000, 
    .ledc_timer = LEDC_TIMER_0, .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_RGB565, .frame_size = FRAMESIZE_96X96, 
    .jpeg_quality = 12, .fb_count = 1, .fb_location = CAMERA_FB_IN_PSRAM, .grab_mode = CAMERA_GRAB_LATEST
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
    Serial.println("INIT_STEP_1: Start Camera Init..."); 
    
    if(psramFound()){
      Serial.println("INIT_STEP_1.1: PSRAM found. Using PSRAM for camera frame buffer.");
    } else {
      Serial.println("INIT_STEP_1.1: PSRAM NOT found.");
      camera_config.fb_location = CAMERA_FB_IN_DRAM;
    }
    
    Serial.println("INIT_STEP_2: Calling esp_camera_init..."); 
    if (esp_camera_init(&camera_config) != ESP_OK) {
        esp_err_t err_code = esp_camera_init(&camera_config);
        Serial.printf("!!! CRITICAL ERROR: Camera Init Failed (0x%x) !!!\n", err_code); 
        return; 
    }
    Serial.println("INIT_STEP_3: Camera initialized successfully."); 

    sensor_t *s = esp_camera_sensor_get();
    if (s != NULL) {
        // Force Fixed Manual Exposure for Stability 
        s->set_gain_ctrl(s, 0); s->set_exposure_ctrl(s, 0); s->set_whitebal(s, 0);
        
        // AGGRESSIVE TUNING: Reduced gain and exposure for closed door stability
        s->set_agc_gain(s, 5);      
        s->set_aec_value(s, 200);    
        s->set_awb_gain(s, 0);       
        
        Serial.println("INIT_STEP_4: Camera settings FIXED (G=5, E=200)."); 
    }
}


// --- IMAGE ACQUISITION AND PRE-PROCESSING ---
bool GetInputData() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) { Serial.println("Camera capture failed."); return false; }
    uint8_t *src_buf = fb->buf; uint16_t src_width = fb->width; uint16_t src_height = fb->height; 
    int8_t *dest_buf = (int8_t *)input->data.int8; size_t dest_index = 0; 
    for (int i = 0; i < src_width * src_height; i++) {
        uint16_t pixel = fb->buf[i * 2] | (fb->buf[i * 2 + 1] << 8); 
        uint8_t r_int8 = ((pixel >> 11) & 0x1F) << 3; 
        uint8_t g_int8 = ((pixel >> 5) & 0x3F) << 2;  
        uint8_t b_int8 = (pixel & 0x1F) << 3;         
        dest_buf[dest_index++] = (int8_t)r_int8 - 128; 
        dest_buf[dest_index++] = (int8_t)g_int8 - 128; 
        dest_buf[dest_index++] = (int8_t)b_int8 - 128; 
    }
    esp_camera_fb_return(fb);
    return true;
}


// --- TFLITE SETUP ---
void setup() {
    delay(1000); 

    init_camera();
    Serial.println("SETUP_STEP_1: Starting TFLite Initialization..."); 

    tflite::InitializeTarget();
    
    static CustomErrorReporter micro_error_reporter; 
    error_reporter = &micro_error_reporter;
    
    model = tflite::GetModel(door_status_model); 
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        MicroPrintf("Model version: %d is not supported by schema version: %d",
                    model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }
    
    // Op Resolver
    static tflite::MicroMutableOpResolver<6> resolver; 
    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddReshape();
    resolver.AddFullyConnected();
    resolver.AddLogistic(); 
    resolver.AddDequantize(); 

    // Allocate the tensor arena explicitly in PSRAM
    Serial.println("SETUP_STEP_2: Allocating Tensor Arena..."); 
    tensor_arena = (uint8_t*)heap_caps_malloc(kTensorArenaSize, MALLOC_CAP_SPIRAM);
    if (tensor_arena == nullptr) {
        MicroPrintf("ERROR: Failed to allocate %d bytes for tensor arena in PSRAM!", kTensorArenaSize);
        return;
    }
    MicroPrintf("SETUP_STEP_3: Arena allocated (%d bytes).", kTensorArenaSize); 

    // Initialize the interpreter
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, (size_t)kTensorArenaSize, nullptr, nullptr, false
    ); 
    interpreter = &static_interpreter;

    // Allocate Tensors
    Serial.println("SETUP_STEP_4: Calling AllocateTensors..."); 
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        MicroPrintf("ERROR: AllocateTensors() failed! Total Arena Size: %d bytes", kTensorArenaSize);
        return;
    }
    
    input = interpreter->input(0);
    output = interpreter->output(0);
    
    Serial.println("SETUP_STEP_5: Model loaded and ready."); 
    Serial.print("Input Scale: "); Serial.println(input->params.scale, 6);
    Serial.print("Input ZeroPoint: "); Serial.println(input->params.zero_point);
}


// --- INFERENCE AND REPORTING LOGIC (FINAL LOGIC) ---
void RunInferenceAndReport() {
    if (!GetInputData()) return;
    if (interpreter->Invoke() != kTfLiteOk) { MicroPrintf("ERROR: Invoke() failed!"); return; }

    int8_t door_open_score_quantized = output->data.int8[DOOR_OPEN_INDEX];
    int8_t door_closed_score_quantized = output->data.int8[DOOR_CLOSED_INDEX];
    
    float door_open_score = (float)(door_open_score_quantized - output->params.zero_point) * output->params.scale;
    float door_closed_score = (float)(door_closed_score_quantized - output->params.zero_point) * output->params.scale;
    
    // --- DEBUG PRINTS ---
    Serial.printf("DEBUG: Scale=%.4f, ZeroPoint=%d\n", output->params.scale, output->params.zero_point);
    Serial.printf("DEBUG: Raw Scores: Closed=%d, Open=%d\n", 
                  door_closed_score_quantized, 
                  door_open_score_quantized);
    // --- DEBUG PRINTS END ---

    // FINAL LOGIC: Detect "OPEN" when the reliable "Closed" score DROPS below the threshold.
    if (door_closed_score < CLOSED_DOOR_THRESHOLD) {
        Serial.printf("🚪 **DOOR OPENED DETECTED!** Confidence: %.2f (Closed Score Drop: < %.2f)\n", 
                      door_open_score, CLOSED_DOOR_THRESHOLD);
    } else {
        // The door is closed if the Closed score is above the threshold.
        Serial.printf("🔒 Door Closed. Highest 'Closed' Score: %.2f ('Open' Score: %.2f)\n", 
                      door_closed_score, door_open_score);
    }
}


// --- MAIN LOOP ---
void loop() {
    RunInferenceAndReport();
    delay(1000); 
}













