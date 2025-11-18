/**********************************************************************************************
 * XIAO ESP32-S3 Image Capture for CNN Dataset (96x96x3)
 * --------------------------------------------------------------------------------------------
 * This sketch captures images from the OV2640 camera attached to the Seeed XIAO ESP32-S3,
 * resizes them to 96x96 RGB, and saves them as JPEG files to the SPI SD card.
 *
 * FEATURES:
 * 1. Prompts the user via Serial Monitor to enter the current date/time at startup.
 *    - Format: YYYY/MM/DD HH:MM:SS
 *    - Used to timestamp the filenames.
 * 2. Prompts the user to enter a label for each batch of images.
 *    - The label is included in each file name: <label>_YYYYMMDD_HHMMSS.jpg
 * 3. Captures images in batches (IMAGE_COUNT), applying a 1-second delay between captures.
 * 4. Resizes each captured frame to 96x96 RGB before saving to SD card.
 * 5. Uses the working SPI SD card configuration:
 *    - CS = 21, SCK = 7, MISO = 8, MOSI = 9
 * 6. Uses the official Seeed XIAO ESP32-S3 OV2640 pinout for camera stability.
 * 7. JPEG quality adjustable via JPEG_QUALITY.
 *
 * REQUIREMENTS:
 * - SD card formatted as FAT32.
 * - Libraries:
 *   - SD.h
 *   - SPI.h
 *   - esp_camera.h
 *   - JPEGDecoder (for resizing)
 *
 * USAGE:
 * 1. Open Serial Monitor at 115200 baud.
 * 2. Enter current date/time when prompted.
 * 3. Enter label for the batch of images.
 * 4. Move/change lighting as needed between captures.
 **********************************************************************************************/

/**********************************************************************************************
 * XIAO ESP32-S3 Image Capture for CNN Dataset (JPEG)
 * --------------------------------------------------------------------------------------------
 * This sketch captures images from the OV2640 camera on the Seeed XIAO ESP32-S3
 * and saves them directly as JPEG files to a SPI SD card.
 *
 * FEATURES:
 * 1. Prompts the user to enter current date/time at startup (YYYY/MM/DD HH:MM:SS)
 *    -> Used for timestamping file names.
 * 2. Prompts the user to enter a label for the batch of images.
 *    -> Included in file names: <label>_YYYYMMDD_HHMMSS.jpg
 * 3. Captures a batch of images (IMAGE_COUNT) with a 1-second interval between captures.
 * 4. Saves raw JPEG frames from the camera directly to SD card.
 * 5. Uses the verified working SPI SD card configuration:
 *    - CS = 21, SCK = 7, MISO = 8, MOSI = 9
 * 6. Uses official Seeed XIAO ESP32-S3 OV2640 pinout for stability.
 **********************************************************************************************/

#include "esp_camera.h"
#include <SD.h>
#include <SPI.h>

// --- USER SETTINGS ---
#define IMAGE_COUNT      10          
#define CAPTURE_DELAY_MS 1000        
#define JPEG_QUALITY     12          

// --- SPI SD CARD CONFIGURATION (WORKING) ---
#define SD_CS   21
#define SD_SCK  7
#define SD_MISO 8
#define SD_MOSI 9

// --- CAMERA PIN CONFIGURATION (Official Seeed XIAO ESP32-S3) ---
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    10
#define CAM_PIN_SIOD    40
#define CAM_PIN_SIOC    39
#define CAM_PIN_D7      48
#define CAM_PIN_D6      11
#define CAM_PIN_D5      12
#define CAM_PIN_D4      14
#define CAM_PIN_D3      16
#define CAM_PIN_D2      18
#define CAM_PIN_D1      17
#define CAM_PIN_D0      15
#define CAM_PIN_VSYNC   38
#define CAM_PIN_HREF    47
#define CAM_PIN_PCLK    13

// --- FUNCTION DECLARATIONS ---
bool initCamera();
String getUserInput();
void setInitialTime();
void generateFilename(const String& label, char* buffer);
bool captureAndSave(const char* filename);

// --- SETUP ---
void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n--- Starting XIAO ESP32-S3 Image Capture ---");

  // --- 1. Initialize SPI SD Card ---
  Serial.println("\n--- Initializing SPI SD Card ---");
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (!SD.begin(SD_CS, SPI)) {
    Serial.println("❌ SD Card init failed! Check FAT32 card.");
    while (true);
  }
  Serial.println("✔ SD init OK!");
  Serial.print("Card type: "); Serial.println(SD.cardType());
  delay(2000);

  // --- 2. Initialize Camera ---
  Serial.println("\n--- Initializing Camera ---");
  if (!initCamera()) {
    Serial.println("❌ Camera init failed! Stopping.");
    while (true);
  }
  Serial.println("✔ Camera initialized.");

  // --- 3. Set RTC time via user input ---
  setInitialTime();

  Serial.println("\n--- Ready for image capture ---");
  Serial.println("Enter the image label for this batch and press Enter:");
}

// --- LOOP ---
void loop() {
  if (Serial.available()) {
    String label = getUserInput();
    if (label.length() == 0) {
      Serial.println("No label entered. Please enter a label:");
      return;
    }
    label.replace(" ", "_");
    Serial.printf("\n*** Starting capture batch for label: %s ***\n", label.c_str());

    for (int i = 1; i <= IMAGE_COUNT; i++) {
      char filename[60];
      generateFilename(label, filename);

      Serial.printf("Capturing image %d/%d... ", i, IMAGE_COUNT);
      if (captureAndSave(filename)) {
        Serial.println("Saved.");
      } else {
        Serial.println("ERROR: Capture/Save failed. Stopping batch.");
        break;
      }

      if (i < IMAGE_COUNT) {
        Serial.printf("Pausing for %d second(s). Move camera/change lighting now.\n", CAPTURE_DELAY_MS / 1000);
        delay(CAPTURE_DELAY_MS);
      }
    }

    Serial.printf("\n*** Batch Complete: %d images saved under label %s ***\n", IMAGE_COUNT, label.c_str());
    Serial.println("Enter the image label for the next batch and press Enter:");
  }
}

// --- FUNCTION DEFINITIONS ---

bool initCamera() {
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer   = LEDC_TIMER_0;
  config.pin_d0       = CAM_PIN_D0;
  config.pin_d1       = CAM_PIN_D1;
  config.pin_d2       = CAM_PIN_D2;
  config.pin_d3       = CAM_PIN_D3;
  config.pin_d4       = CAM_PIN_D4;
  config.pin_d5       = CAM_PIN_D5;
  config.pin_d6       = CAM_PIN_D6;
  config.pin_d7       = CAM_PIN_D7;
  config.pin_xclk     = CAM_PIN_XCLK;
  config.pin_pclk     = CAM_PIN_PCLK;
  config.pin_vsync    = CAM_PIN_VSYNC;
  config.pin_href     = CAM_PIN_HREF;
  config.pin_sscb_sda = CAM_PIN_SIOD;
  config.pin_sscb_scl = CAM_PIN_SIOC;
  config.pin_pwdn     = CAM_PIN_PWDN;
  config.pin_reset    = CAM_PIN_RESET;
  config.xclk_freq_hz = 10000000; // Lower XCLK to reduce crashes
  config.pixel_format = PIXFORMAT_JPEG;
  config.frame_size   = FRAMESIZE_QVGA; // 320x240, can resize in Python later
  config.jpeg_quality = JPEG_QUALITY;
  config.fb_count     = 1;
  config.grab_mode    = CAMERA_GRAB_LATEST;

  esp_err_t err = esp_camera_init(&config);
  return (err == ESP_OK);
}

String getUserInput() {
  while (!Serial.available()) delay(10);
  String input = Serial.readStringUntil('\n');
  input.trim();
  return input;
}

void setInitialTime() {
  Serial.println("\nEnter current date and time (YYYY/MM/DD HH:MM:SS):");
  String input = getUserInput();

  struct tm t;
  if (strptime(input.c_str(), "%Y/%m/%d %H:%M:%S", &t) == NULL) {
    Serial.println("Invalid format, using compile time.");
    time_t compile_time = time(NULL);
    t = *localtime(&compile_time);
  }

  time_t now = mktime(&t);
  struct timeval tv = {.tv_sec = now, .tv_usec = 0};
  settimeofday(&tv, NULL);

  Serial.print("RTC set to: ");
  Serial.println(asctime(localtime(&now)));
}

void generateFilename(const String& label, char* buffer) {
  time_t now = time(NULL);
  struct tm* timeinfo = localtime(&now);
  strftime(buffer, 60, "/%Y%m%d_%H%M%S.jpg", timeinfo);
  String temp = String(buffer);
  temp = "/" + label + temp.substring(1);
  temp.toCharArray(buffer, 60);
}

bool captureAndSave(const char* filename) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) return false;

  File file = SD.open(filename, FILE_WRITE);
  if (!file) {
    esp_camera_fb_return(fb);
    return false;
  }
  file.write(fb->buf, fb->len);
  file.close();

  esp_camera_fb_return(fb);
  return true;
}
