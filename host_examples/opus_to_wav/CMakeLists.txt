cmake_minimum_required(VERSION 3.16)
project(opus_to_wav CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Option to enable sanitizers for debugging
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

# Configure sanitizers if enabled
if(ENABLE_SANITIZERS)
    message(STATUS "Building with AddressSanitizer and UndefinedBehaviorSanitizer")
    set(SANITIZER_FLAGS "-fsanitize=address,undefined -fno-omit-frame-pointer -g")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${SANITIZER_FLAGS}")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${SANITIZER_FLAGS}")
endif()

# Add microOggDemuxer library first
# Check if it's already been added
if(NOT TARGET micro_ogg_demuxer)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../../lib/micro-ogg-demuxer
                     ${CMAKE_CURRENT_BINARY_DIR}/micro-ogg-demuxer)
endif()

# Add microOpus as a subdirectory
# This will build it as a standard library (not ESP-IDF component)
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../.. micro-opus-build)

# Create opus_to_wav executable
add_executable(opus_to_wav
    opus_to_wav.cpp
    wav_writer.cpp
)

# Link against micro_opus library
target_link_libraries(opus_to_wav PRIVATE
    micro_opus
)

# Install target
install(TARGETS opus_to_wav
    RUNTIME DESTINATION bin
)
