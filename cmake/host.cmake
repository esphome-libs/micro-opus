# cmake/host.cmake
# Host platform build configuration for microOpus

# Guard against multiple inclusion
if(__opus_host_defined)
    return()
endif()
set(__opus_host_defined TRUE)

# ==============================================================================
# opus_configure_host
# ==============================================================================
# Main configuration function for host builds (Linux, macOS, Windows).
# Call this after creating the library target to set up all host-specific
# configuration.
#
# Arguments:
#   TARGET          - The library target name
#   SOURCE_DIR      - The source directory path (CMAKE_CURRENT_SOURCE_DIR)
#   OPUS_STAGED_DIR - Path to the staged (patched) opus directory
# ==============================================================================
function(opus_configure_host TARGET SOURCE_DIR OPUS_STAGED_DIR)
    # Add micro-ogg-demuxer as a subdirectory
    if(NOT TARGET micro_ogg_demuxer)
        add_subdirectory(${SOURCE_DIR}/lib/micro-ogg-demuxer
                         ${CMAKE_CURRENT_BINARY_DIR}/micro-ogg-demuxer)
    endif()
    target_link_libraries(${TARGET} PUBLIC micro_ogg_demuxer)

    # Include directories - use staged opus directory
    target_include_directories(${TARGET} PUBLIC
        ${OPUS_STAGED_DIR}/include
        ${SOURCE_DIR}/include
    )

    target_include_directories(${TARGET} PRIVATE
        ${OPUS_STAGED_DIR}
        ${OPUS_STAGED_DIR}/celt
        ${OPUS_STAGED_DIR}/silk
        ${OPUS_STAGED_DIR}/silk/fixed
        ${SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}
    )

    # Add patches directory for custom headers (custom_support.h, thread_local_stack.h)
    target_include_directories(${TARGET} BEFORE PRIVATE
        ${SOURCE_DIR}/patches
    )

    # Set common definitions
    opus_set_common_definitions(${TARGET})

    # Host-specific: fixed-point only
    target_compile_definitions(${TARGET} PRIVATE
        FIXED_POINT=1
        DISABLE_FLOAT_API
    )

    # Configure memory allocation mode
    _opus_configure_host_allocation(${TARGET})

    # Set optimization flags
    opus_set_optimization_flags(${TARGET})

    # Link pthread for thread-local storage
    find_package(Threads REQUIRED)
    target_link_libraries(${TARGET} PRIVATE Threads::Threads)

    # Generate config.h
    set(SIZEOF_LONG 8)  # 64-bit on most host platforms
    configure_file(
        "${SOURCE_DIR}/cmake/config.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/config.h"
        @ONLY
    )

    message(STATUS "Opus: Building for host platform (fixed-point)")
endfunction()

# ==============================================================================
# Internal helper functions
# ==============================================================================

# Configure memory allocation mode for host builds
function(_opus_configure_host_allocation TARGET)
    if(OPUS_ALLOCATION_MODE STREQUAL "THREADSAFE_PSEUDOSTACK")
        target_compile_definitions(${TARGET} PRIVATE THREADSAFE_PSEUDOSTACK)
        message(STATUS "Opus (host): Using THREADSAFE_PSEUDOSTACK mode")
    elseif(OPUS_ALLOCATION_MODE STREQUAL "NONTHREADSAFE_PSEUDOSTACK")
        target_compile_definitions(${TARGET} PRIVATE NONTHREADSAFE_PSEUDOSTACK)
        message(STATUS "Opus (host): Using NONTHREADSAFE_PSEUDOSTACK mode")
    elseif(OPUS_ALLOCATION_MODE STREQUAL "USE_ALLOCA")
        target_compile_definitions(${TARGET} PRIVATE USE_ALLOCA)
        message(STATUS "Opus (host): Using USE_ALLOCA mode")
    else()
        message(FATAL_ERROR "Invalid OPUS_ALLOCATION_MODE: ${OPUS_ALLOCATION_MODE}")
    endif()
endfunction()
