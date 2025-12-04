#!/bin/bash
# Build opus_to_wav and tests with AddressSanitizer and UndefinedBehaviorSanitizer

set -e

# Clean build directory
rm -rf build
mkdir build
cd build

# Configure and build with sanitizers
cmake -DENABLE_SANITIZERS=ON ..
cmake --build .

echo ""
echo "Build complete. Run tests with:"
echo "  ./opus_to_wav input.opus output.wav"
echo "  ./test_chunked input.opus"
echo "  ./test_silent_channels"
echo "  ./measure_zerocopy input.opus"
