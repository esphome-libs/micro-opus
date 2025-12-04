#!/usr/bin/env python3
"""
Convert an Opus audio file to a C header file with byte array.
"""

import sys
import os
import argparse

def convert_opus_to_header(opus_file, header_file, array_name, description):
    """Convert opus file to C header with byte array."""

    # Read the opus file
    with open(opus_file, 'rb') as f:
        opus_data = f.read()

    # Generate guard name from header file
    guard_name = os.path.basename(header_file).upper().replace('.', '_')

    # Generate header file
    with open(header_file, 'w') as f:
        f.write("/* Auto-generated from {} */\n".format(os.path.basename(opus_file)))
        f.write("/* File size: {} bytes */\n".format(len(opus_data)))
        if description:
            f.write("/*\n")
            for line in description.strip().split('\n'):
                f.write(" * {}\n".format(line))
            f.write(" */\n")
        f.write("\n")
        f.write("#ifndef {}\n".format(guard_name))
        f.write("#define {}\n\n".format(guard_name))
        f.write("#include <stdint.h>\n\n")

        # Write the data array
        f.write("static const uint8_t {}[] = {{\n".format(array_name))

        # Format with 16 bytes per line
        for i in range(0, len(opus_data), 16):
            chunk = opus_data[i:i+16]
            hex_values = ', '.join('0x{:02X}'.format(b) for b in chunk)
            f.write("    {}".format(hex_values))
            if i + 16 < len(opus_data):
                f.write(",\n")
            else:
                f.write("\n")

        f.write("};\n\n")
        f.write("static const size_t {}_size = sizeof({});\n\n".format(array_name, array_name))
        f.write("#endif // {}\n".format(guard_name))

    print(f"Converted {opus_file} to {header_file}")
    print(f"Array name: {array_name}")
    print(f"Data size: {len(opus_data)} bytes")

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Convert Opus file to C header')
    parser.add_argument('input', help='Input .opus file')
    parser.add_argument('output', help='Output .h file')
    parser.add_argument('--name', '-n', default='test_opus_data',
                        help='Name for the data array (default: test_opus_data)')
    parser.add_argument('--description', '-d', default='',
                        help='Multi-line description comment for the header')
    args = parser.parse_args()

    if not os.path.exists(args.input):
        print(f"Error: Input file '{args.input}' not found")
        sys.exit(1)

    convert_opus_to_header(args.input, args.output, args.name, args.description)
