import os
import zipfile
import shutil

# --------------- CONFIG -----------------
project_path = r"C:\Users\Kris\Desktop\Edge_AI\tinyml_xiao_project"
tflite_zip_path = r"C:\Users\Kris\Desktop\Edge_AI\tflite-micro-main.zip"
tflite_model_path = r"C:\Users\Kris\Desktop\Edge_AI\person_model.h"
# ----------------------------------------

# 1️⃣ Extract TFLite Micro library
lib_dest = os.path.join(project_path, "lib", "tflite-micro")
if not os.path.exists(lib_dest):
    os.makedirs(lib_dest)

with zipfile.ZipFile(tflite_zip_path, 'r') as zip_ref:
    zip_ref.extractall(lib_dest)

print(f"TFLite Micro library extracted to {lib_dest}")

# 2️⃣ Copy your model header
model_cc_path = os.path.join(project_path, "src", "model.cc")
shutil.copyfile(tflite_model_path, model_cc_path)
print(f"Copied {tflite_model_path} to {model_cc_path}")

print("Project is now ready to open in PlatformIO!")


