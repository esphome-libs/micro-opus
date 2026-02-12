# CMakeLists.txt for microOpus
# Supports both ESP-IDF component builds and standalone host builds
#
# This build system uses a staging approach:
# 1. Copies the opus submodule to the build directory
# 2. Applies patches using standard .patch files
# 3. Builds from the staged (patched) copy
#
# This keeps the original submodule pristine.
#
# Structure:
#   cmake/sources.cmake   - Source file definitions (function-based)
#   cmake/functions.cmake - Helper functions
#   cmake/staging.cmake   - Staging directory and patching system
#   cmake/config.h.in     - Config header template
#   cmake/esp-idf.cmake   - ESP-IDF specific configuration
#   cmake/host.cmake      - Host platform configuration
#   patches/diffs/        - Patch files (.patch format)

# ==============================================================================
# Build type detection
# ==============================================================================

if(DEFINED IDF_TARGET)
    set(ESP_IDF_BUILD TRUE)
else()
    set(ESP_IDF_BUILD FALSE)
endif()

# Set source directory variable for consistent use
if(ESP_IDF_BUILD)
    set(OPUS_COMPONENT_DIR ${COMPONENT_DIR})
else()
    set(OPUS_COMPONENT_DIR ${CMAKE_CURRENT_SOURCE_DIR})
endif()

# ==============================================================================
# Include CMake modules
# ==============================================================================

include(${OPUS_COMPONENT_DIR}/cmake/dependencies.cmake)
include(${OPUS_COMPONENT_DIR}/cmake/sources.cmake)
include(${OPUS_COMPONENT_DIR}/cmake/functions.cmake)
include(${OPUS_COMPONENT_DIR}/cmake/staging.cmake)

# ==============================================================================
# ESP-IDF Build
# ==============================================================================

if(ESP_IDF_BUILD)
    include(${OPUS_COMPONENT_DIR}/cmake/esp-idf.cmake)

    # Get IDF target
    idf_build_get_property(target IDF_TARGET)

    # Setup staged build directory with patches
    # Apply Xtensa patches if enabled via Kconfig (default on for ESP32/ESP32-S3)
    if(CONFIG_OPUS_ENABLE_XTENSA_OPTIMIZATIONS)
        opus_setup_staged_build(${COMPONENT_DIR} TRUE)
    else()
        opus_setup_staged_build(${COMPONENT_DIR} FALSE)
    endif()

    # Get sources using staged directory
    opus_get_sources(${OPUS_STAGED_DIR})

    # Collect all sources
    set(ESP_OPUS_SOURCES
        ${OPUS_BASE_SOURCES}
        ${OPUS_DECODER_SOURCES}
        ${OPUS_ENCODER_SOURCES}
        ${OGG_OPUS_SOURCES}
        ${CELT_SOURCES}
        ${SILK_BASE_SOURCES}
    )

    # Add thread-local storage if needed
    if(CONFIG_OPUS_THREADSAFE_PSEUDOSTACK)
        list(APPEND ESP_OPUS_SOURCES ${THREAD_LOCAL_SOURCES})
    endif()

    # Register the component
    idf_component_register(
        SRCS ${ESP_OPUS_SOURCES}
        INCLUDE_DIRS
            "${OPUS_STAGED_DIR}/include"
            "include"
            "${OPUS_STAGED_DIR}"
            "${OPUS_STAGED_DIR}/celt"
            "${OPUS_STAGED_DIR}/silk"
            "${OPUS_STAGED_DIR}/silk/float"
            "${OPUS_STAGED_DIR}/silk/fixed"
            "."
        PRIV_REQUIRES esp_timer     # Needed for timing profilers
    )

    # Apply ESP-IDF configuration
    opus_configure_esp_idf(${COMPONENT_LIB} ${COMPONENT_DIR} ${OPUS_STAGED_DIR})

# ==============================================================================
# Host Build
# ==============================================================================

else()
    cmake_minimum_required(VERSION 3.16)
    project(micro_opus C CXX)

    include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/host.cmake)

    # Option for memory allocation mode
    set(OPUS_ALLOCATION_MODE "THREADSAFE_PSEUDOSTACK" CACHE STRING "Memory allocation mode")
    set_property(CACHE OPUS_ALLOCATION_MODE PROPERTY STRINGS
        "THREADSAFE_PSEUDOSTACK"
        "NONTHREADSAFE_PSEUDOSTACK"
        "USE_ALLOCA"
    )

    # Setup staged build directory (no Xtensa patches for host)
    opus_setup_staged_build(${CMAKE_CURRENT_SOURCE_DIR} FALSE)

    # Get sources using staged directory
    opus_get_sources(${OPUS_STAGED_DIR})

    # Collect all sources
    set(HOST_OPUS_SOURCES
        ${OPUS_BASE_SOURCES}
        ${OPUS_DECODER_SOURCES}
        ${OPUS_ENCODER_SOURCES}
        ${OGG_OPUS_SOURCES}
        ${CELT_SOURCES}
        ${SILK_BASE_SOURCES}
        ${SILK_FIXED_SOURCES}
    )

    # Add thread-local storage if needed
    if(OPUS_ALLOCATION_MODE STREQUAL "THREADSAFE_PSEUDOSTACK")
        list(APPEND HOST_OPUS_SOURCES ${THREAD_LOCAL_SOURCES})
    endif()

    # Create the library
    add_library(micro_opus STATIC ${HOST_OPUS_SOURCES})

    # Apply host configuration
    opus_configure_host(micro_opus ${CMAKE_CURRENT_SOURCE_DIR} ${OPUS_STAGED_DIR})

endif()
