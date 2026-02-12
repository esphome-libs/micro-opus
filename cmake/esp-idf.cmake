# cmake/esp-idf.cmake
# ESP-IDF specific build configuration for microOpus

# Guard against multiple inclusion
if(__opus_esp_idf_defined)
    return()
endif()
set(__opus_esp_idf_defined TRUE)

# ==============================================================================
# opus_configure_esp_idf
# ==============================================================================
# Main configuration function for ESP-IDF builds. Call this after
# idf_component_register() to set up all ESP-IDF specific configuration.
#
# Arguments:
#   COMPONENT_LIB  - The component library target name
#   COMPONENT_DIR  - The component directory path
#   OPUS_STAGED_DIR - Path to the staged (patched) opus directory
# ==============================================================================
function(opus_configure_esp_idf COMPONENT_LIB COMPONENT_DIR OPUS_STAGED_DIR)
    # Get IDF target
    idf_build_get_property(target IDF_TARGET)

    # Force disable PIC/PIE at compile level for RISC-V (these must come early)
    # Also use local-exec TLS model to avoid GOT-relative TLS access
    if(NOT target STREQUAL "esp32" AND NOT target STREQUAL "esp32s3" AND NOT target STREQUAL "esp32s2")
        # Use BEFORE to ensure these flags come before any -fPIC that might be added later
        target_compile_options(${COMPONENT_LIB} BEFORE PUBLIC
            -fno-pic -fno-pie -fno-plt -ftls-model=local-exec
        )
    endif()

    # Add micro-ogg-demuxer as a subdirectory
    if(NOT TARGET micro_ogg_demuxer)
        add_subdirectory(${COMPONENT_DIR}/lib/micro-ogg-demuxer
                         ${CMAKE_CURRENT_BINARY_DIR}/micro-ogg-demuxer)
    endif()
    target_link_libraries(${COMPONENT_LIB} PUBLIC micro_ogg_demuxer)

    # Add patches directory to include path for custom headers
    # (custom_support.h, thread_local_stack.h, timing headers)
    target_include_directories(${COMPONENT_LIB} BEFORE PRIVATE
        "${COMPONENT_DIR}/patches"
        "${COMPONENT_DIR}/src"
    )

    # Set common definitions
    opus_set_common_definitions(${COMPONENT_LIB})

    # Configure memory allocation mode
    _opus_configure_allocation_mode(${COMPONENT_LIB})

    # Configure timing instrumentation if enabled
    _opus_configure_timing(${COMPONENT_LIB})

    # Configure fixed-point vs floating-point
    _opus_configure_float_mode(${COMPONENT_LIB} ${target} ${OPUS_STAGED_DIR})

    # Set optimization flags
    opus_set_optimization_flags(${COMPONENT_LIB})

    # Configure Xtensa optimizations (controlled via Kconfig)
    if(CONFIG_OPUS_ENABLE_XTENSA_OPTIMIZATIONS)
        target_compile_definitions(${COMPONENT_LIB} PRIVATE OPUS_XTENSA_LX7)
        target_sources(${COMPONENT_LIB} PRIVATE
            "${OPUS_STAGED_DIR}/celt/xtensa/mathops_lx7.c"
        )
        message(STATUS "Opus: Xtensa optimizations enabled")
    endif()

    # Generate config.h
    set(SIZEOF_LONG 4)  # 32-bit on ESP32
    configure_file(
        "${COMPONENT_DIR}/cmake/config.h.in"
        "${CMAKE_CURRENT_BINARY_DIR}/config.h"
        @ONLY
    )
    target_include_directories(${COMPONENT_LIB} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endfunction()

# ==============================================================================
# Internal helper functions
# ==============================================================================

# Configure memory allocation mode based on Kconfig options
function(_opus_configure_allocation_mode TARGET)
    if(CONFIG_OPUS_THREADSAFE_PSEUDOSTACK)
        target_compile_definitions(${TARGET} PRIVATE
            THREADSAFE_PSEUDOSTACK
            GLOBAL_STACK_SIZE=${CONFIG_OPUS_PSEUDOSTACK_SIZE}
        )
        message(STATUS "Opus: Using THREADSAFE_PSEUDOSTACK mode with ${CONFIG_OPUS_PSEUDOSTACK_SIZE} byte pseudostack")
    elseif(CONFIG_OPUS_NONTHREADSAFE_PSEUDOSTACK)
        target_compile_definitions(${TARGET} PRIVATE
            NONTHREADSAFE_PSEUDOSTACK
            GLOBAL_STACK_SIZE=${CONFIG_OPUS_PSEUDOSTACK_SIZE}
        )
        message(STATUS "Opus: Using NONTHREADSAFE_PSEUDOSTACK mode with ${CONFIG_OPUS_PSEUDOSTACK_SIZE} byte pseudostack")
    elseif(CONFIG_OPUS_USE_ALLOCA)
        target_compile_definitions(${TARGET} PRIVATE USE_ALLOCA)
        message(STATUS "Opus: Using USE_ALLOCA mode (stack allocation)")
    else()
        message(FATAL_ERROR "No Opus memory allocation mode selected!")
    endif()
endfunction()

# Configure timing instrumentation
function(_opus_configure_timing TARGET)
    if(CONFIG_OPUS_ENABLE_CELT_TIMING)
        target_compile_definitions(${TARGET} PRIVATE CONFIG_OPUS_ENABLE_CELT_TIMING)
        message(STATUS "Opus: CELT timing instrumentation enabled")
    endif()

    if(CONFIG_OPUS_ENABLE_PVQ_TIMING)
        target_compile_definitions(${TARGET} PRIVATE CONFIG_OPUS_ENABLE_PVQ_TIMING)
        message(STATUS "Opus: PVQ timing instrumentation enabled")
    endif()

    if(CONFIG_OPUS_ENABLE_QUANT_BANDS_TIMING)
        target_compile_definitions(${TARGET} PRIVATE CONFIG_OPUS_ENABLE_QUANT_BANDS_TIMING)
        message(STATUS "Opus: quant_all_bands timing instrumentation enabled")
    endif()
endfunction()

# Configure floating-point vs fixed-point mode
function(_opus_configure_float_mode TARGET IDF_TARGET OPUS_STAGED_DIR)
    # Floating-point build (user is trusted to enable only on platforms with FPU)
    if(CONFIG_OPUS_FLOATING_POINT)
        # Add float sources from staged directory
        target_sources(${TARGET} PRIVATE
            "${OPUS_STAGED_DIR}/silk/float/apply_sine_window_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/corrMatrix_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/encode_frame_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/find_LPC_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/find_LTP_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/find_pitch_lags_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/find_pred_coefs_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/LPC_analysis_filter_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/LTP_analysis_filter_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/LTP_scale_ctrl_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/noise_shape_analysis_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/process_gains_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/regularize_correlations_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/residual_energy_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/warped_autocorrelation_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/wrappers_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/autocorrelation_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/burg_modified_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/bwexpander_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/energy_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/inner_product_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/k2a_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/LPC_inv_pred_gain_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/pitch_analysis_core_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/scale_copy_vector_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/scale_vector_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/schur_FLP.c"
            "${OPUS_STAGED_DIR}/silk/float/sort_FLP.c"
        )
        target_compile_definitions(${TARGET} PRIVATE
            OPUS_ENABLE_FLOAT_API
            FLOATING_POINT
            ESP_PLATFORM
            FLOAT_APPROX
        )
        target_compile_options(${TARGET} PRIVATE
            -ffast-math
            -fno-finite-math-only
        )
        message(STATUS "Opus: Using floating-point implementation for ${IDF_TARGET}")
    else()
        # Fixed-point build (default for all ESP32 variants)
        target_sources(${TARGET} PRIVATE ${SILK_FIXED_SOURCES})
        target_compile_definitions(${TARGET} PRIVATE
            FIXED_POINT=1
            DISABLE_FLOAT_API
        )
        message(STATUS "Opus: Using fixed-point implementation")
    endif()
endfunction()
