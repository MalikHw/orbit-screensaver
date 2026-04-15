#!/usr/bin/env python3
"""
bin2h.py  <input_file>  <output_header>  <variable_name>
Converts any binary file into a C header with a const unsigned char array.
"""
import sys, os

def main():
    if len(sys.argv) != 4:
        print(f"usage: {sys.argv[0]} <input> <output.h> <varname>")
        sys.exit(1)

    inp, out, varname = sys.argv[1], sys.argv[2], sys.argv[3]

    with open(inp, "rb") as f:
        data = f.read()

    lines = [
        f"// auto-generated from {os.path.basename(inp)} — do not edit",
        f"#pragma once",
        f"static const unsigned char {varname}[] = {{",
    ]

    row = []
    for i, b in enumerate(data):
        row.append(f"0x{b:02x}")
        if len(row) == 16:
            lines.append("    " + ", ".join(row) + ",")
            row = []
    if row:
        lines.append("    " + ", ".join(row) + ",")

    lines += [
        "};",
        f"static const unsigned int {varname}_len = {len(data)};",
        "",
    ]

    with open(out, "w") as f:
        f.write("\n".join(lines))

    print(f"[bin2h] {inp} -> {out}  ({len(data)} bytes)")

if __name__ == "__main__":
    main()
