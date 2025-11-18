import argparse

# Define the expected array name in the Arduino sketch
ARRAY_NAME = "door_status_model" 

def generate_header(tflite_path, header_path, array_name):
    with open(tflite_path, 'rb') as f:
        tflite_model = f.read()

    # Format the model data as a C array
    hex_data = ', '.join([f'0x{byte:02x}' for byte in tflite_model])

    # Write the C header file
    with open(header_path, 'w') as f:
        f.write(f'#ifndef MODEL_H\n')
        f.write(f'#define MODEL_H\n\n')
        f.write(f'// Model size: {len(tflite_model)} bytes\n')
        f.write(f'const unsigned char {array_name}[] = {{\n')
        f.write(f'  {hex_data}\n')
        f.write(f'}};\n\n')
        f.write(f'const int {array_name}_len = {len(tflite_model)};\n\n')
        f.write(f'#endif // MODEL_H\n')

    print(f"Successfully generated {header_path} ({len(tflite_model)} bytes) with array name '{array_name}'.")

# Define the names of your files
input_file = "door_status_model.tflite"
output_file = "model.h"

# NOTE: The Arduino sketch expects the variable name 'person_model', 
# even if your input file is 'door_status_model.tflite'.
generate_header(input_file, output_file, ARRAY_NAME)