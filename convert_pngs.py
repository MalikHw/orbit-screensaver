#!/usr/bin/env python3

import os
import sys

def png_to_header(png_path, output_path):
    with open(png_path, 'rb') as f:
        data = f.read()
    

    base_name = os.path.splitext(os.path.basename(png_path))[0]
    var_name = base_name.replace('.', '_')
    
    with open(output_path, 'w') as f:
        f.write(f"static const unsigned char {var_name}_png[] = {{\n")
        for i, byte in enumerate(data):
            if i % 12 == 0:
                f.write("    ")
            f.write(f"0x{byte:02x},")
            if (i + 1) % 12 == 0:
                f.write("\n")
        if len(data) % 12 != 0:
            f.write("\n")
        f.write("};\n")
        f.write(f"static const size_t {var_name}_png_len = {len(data)};\n")

def main():
    png_files = [
        'orb1.png', 'orb2.png', 'orb3.png', 'orb4.png', 'orb5.png',
        'orb6.png', 'orb7.png', 'orb8.png', 'orb9.png', 'orb10.png',
        'orb11.png', 'cube.png'
    ]
    
    for png_file in png_files:
        if os.path.exists(png_file):
            header_file = os.path.splitext(png_file)[0] + '_data.h'
            print(f"Converting {png_file} to {header_file}")
            png_to_header(png_file, header_file)
        else:
            print(f"Warning: {png_file} not found", file=sys.stderr)

if __name__ == "__main__":
    main()
