import os
import shutil

# --- CONFIGURATION & PATH SETUP ---
# Get the directory where this script is located
script_dir = os.path.dirname(os.path.abspath(__file__))

# 1. Project paths
project_name = "tinyml_xiao_project"
project_path = os.path.join(script_dir, project_name)

# Model source and destination paths
# 📢 This line grabs person_model.h from the same directory as this script.
model_source_path = os.path.join(script_dir, "person_model.h")
src_path = os.path.join(project_path, "src")
model_dest_path = os.path.join(src_path, "model.cc")

# Library path (where tflite-micro would be placed/extracted)
lib_path = os.path.join(project_path, "lib", "tflite-micro")

# 2. Create folders
print(f"Creating project structure in: {project_path}")
os.makedirs(src_path, exist_ok=True)
os.makedirs(lib_path, exist_ok=True)

# 3. platformio.ini content (with the crucial TFLite-Micro build fix)
platformio_ini = """[env:xiao_esp32s3]
platform = espressif32
board = seeed_xiao_esp32s3
framework = arduino
monitor_speed = 115200

; --- TFLITE-MICRO BUILD FIX ---
; These flags prevent compilation errors by excluding TFLite-Micro test files 
; (like test_simd.c) that require missing dependencies (like kiss_fftnd.h).
build_unflags =
    -D_GLIBCXX_HAVE_ICONV
    -D__STDC_LIMIT_MACROS
    -D__STDC_CONSTANT_MACROS
    -D__STDC_VERSION__
    -D_GNU_SOURCE

build_flags =
    -DBOARD_HAS_PSRAM
    -DCONFIG_LITTLEFS_FOR_IDF_3_2
    -Wl,--gc-sections -Wl,--start-group -lstdc++ -lsupc++ -lgcc -lnosys -Wl,--end-group
    -D_LIBC_REENTRANT
    -DTF_LITE_STATIC_MEMORY
    -D__OPTIMIZE_SIZE__
lib_deps =
  # No external library needed; TFLite Micro will be added locally
"""

platformio_ini_path = os.path.join(project_path, "platformio.ini")
with open(platformio_ini_path, "w") as f:
    f.write(platformio_ini)
print(f"Created platformio.ini with build fixes at {platformio_ini_path}")

# 4. Copy person_model.h and rename to model.cc
print("--- Copying Model Header ---")
if os.path.exists(model_source_path):
    # Copy the file from the script directory to the project/src directory
    shutil.copyfile(model_source_path, model_dest_path)
    print(f"Copied '{os.path.basename(model_source_path)}' to '{os.path.basename(model_dest_path)}'.")
else:
    print(f"⚠️ Warning: 'person_model.h' not found in the script directory. Creating empty model.cc placeholder.")
    # Create an empty placeholder if the model file is missing
    with open(model_dest_path, "w") as f:
        f.write("// DROP YOUR MODEL.TFLITE C ARRAY HERE AS A model.cc FILE\n")


# 5. Minimal main.cpp
main_cpp = """#include <Arduino.h>
#include \"model.cc\"
#include \"tensorflow/lite/micro/all_ops_resolver.h\"
#include \"tensorflow/lite/micro/micro_interpreter.h\"
#include \"tensorflow/lite/schema/schema_generated.h\"
#include \"tensorflow/lite/version.h\"

// Adjust this size based on your model's needs. 10KB is a common starting point.
constexpr int kTensorArenaSize = 10 * 1024; 
uint8_t tensor_arena[kTensorArenaSize];

tflite::MicroInterpreter* interpreter;

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println(\"\\n--- TENSORFLOW LITE MICRO START ---\");

  const tflite::Model* model = tflite::GetModel(model_tflite);
  if (model->version() != TFLITE_SCHEMA_VERSION) {
    Serial.println(\"Model schema version is not a match!\");
    return;
  }
  
  static tflite::AllOpsResolver resolver;
  static tflite::MicroInterpreter static_interpreter(
      model, resolver, tensor_arena, kTensorArenaSize);
  interpreter = &static_interpreter;

  if (interpreter->AllocateTensors() != kTfLiteOk) {
    Serial.println(\"AllocateTensors() failed!\");
  } else {
    Serial.println(\"Model loaded and tensors allocated!\");
  }
}

void loop() {
  // If your model has inputs, you would populate them here:
  // float* input = interpreter->input(0)->data.f;
  
  if (interpreter->Invoke() != kTfLiteOk) {
    Serial.println(\"Invoke failed!\");
  } else {
    // Read and print your model's output here:
    // Serial.print(\"Output[0]: \");
    // Serial.println(interpreter->output(0)->data.f[0]);
  }

  delay(1000);
}
"""

main_cpp_path = os.path.join(src_path, "main.cpp")
with open(main_cpp_path, "w") as f:
    f.write(main_cpp)
print(f"Created main.cpp at {main_cpp_path}")

print("\n✨ Setup Complete! ✨")
print(f"Project '{project_name}' is ready to be built using your PowerShell script.")
