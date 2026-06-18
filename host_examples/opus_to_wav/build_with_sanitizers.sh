#!/bin/bash
# Build opus_to_wav with AddressSanitizer and UndefinedBehaviorSanitizer

set -e

# Clean build directory
rm -rf build
mkdir build
cd build

# Configure and build with sanitizers
cmake -DENABLE_SANITIZERS=ON ..
cmake --build .

echo ""
echo "Build complete. Run the example with:"
echo "  ./build/opus_to_wav input.opus output.wav"
