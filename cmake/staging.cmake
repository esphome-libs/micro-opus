# cmake/staging.cmake
# Staging directory and patching system for microOpus
#
# This module copies the opus submodule to the build directory and applies
# patches there, keeping the original submodule pristine.

# Guard against multiple inclusion
if(__opus_staging_defined)
    return()
endif()
set(__opus_staging_defined TRUE)

# ==============================================================================
# Configuration
# ==============================================================================

# Staging directory location (in build directory)
set(OPUS_STAGED_DIR "${CMAKE_CURRENT_BINARY_DIR}/opus-staged")

# Patch files directory
set(OPUS_PATCHES_DIR "${OPUS_COMPONENT_DIR}/patches")
set(OPUS_DIFFS_DIR "${OPUS_PATCHES_DIR}/diffs")

# ==============================================================================
# Patch definitions
# ==============================================================================

# Core patches (always applied)
set(OPUS_CORE_PATCHES
    celt_stack_alloc.patch
)

# ESP32-S3 Xtensa LX7 patches (applied only for esp32s3 target)
# Note: silk_macros.patch and silk_SigProc_FIX.patch removed - now in upstream v1.6
set(OPUS_XTENSA_PATCHES
    celt_arch.patch
    celt_celt.patch
    celt_mathops.patch
    celt_pitch.patch
    silk_SigProc_FLP.patch
)

# Additional files to copy (not patches, just new files)
# Note: silk/xtensa/macros_lx7.h and SigProc_FIX_lx7.h removed - now in upstream v1.6
set(OPUS_XTENSA_ADDITIONS
    # CELT Xtensa headers
    celt/xtensa/fixed_lx7.h
    celt/xtensa/mathops_lx7.h
    celt/xtensa/mathops_lx7.c
    celt/xtensa/pitch_lx7.h
    # SILK Xtensa headers (floating-point only - fixed-point now in upstream)
    silk/xtensa/SigProc_FLP_lx7.h
)

# ==============================================================================
# opus_create_staging_directory
# ==============================================================================
# Creates a staged copy of the opus submodule in the build directory.
# This is called once during CMake configure. Re-staging is triggered when:
# - The source submodule changes (timestamp check)
# - Build configuration changes (Xtensa, timing options)
#
# Arguments:
#   SOURCE_DIR   - Path to the original opus submodule (lib/opus)
#   STAGED_DIR   - Path where the staged copy will be created
#   APPLY_XTENSA - Whether Xtensa patches will be applied
# ==============================================================================
function(opus_create_staging_directory SOURCE_DIR STAGED_DIR APPLY_XTENSA)
    # Check if staging is needed (source newer than staged, or staged doesn't exist)
    set(STAGING_MARKER "${STAGED_DIR}/.staging_complete")

    # Get source directory modification time (use a key file)
    file(TIMESTAMP "${SOURCE_DIR}/include/opus.h" SOURCE_TIMESTAMP "%Y%m%d%H%M%S" UTC)

    # Build a configuration hash to detect option changes
    # Include all options that affect which patches/files are applied
    set(CONFIG_STRING "${SOURCE_TIMESTAMP}")
    set(CONFIG_STRING "${CONFIG_STRING}_xtensa=${APPLY_XTENSA}")
    set(CONFIG_STRING "${CONFIG_STRING}_xtensa_kconfig=${CONFIG_OPUS_ENABLE_XTENSA_OPTIMIZATIONS}")
    set(CONFIG_STRING "${CONFIG_STRING}_celt_timing=${CONFIG_OPUS_ENABLE_CELT_TIMING}")
    set(CONFIG_STRING "${CONFIG_STRING}_pvq_timing=${CONFIG_OPUS_ENABLE_PVQ_TIMING}")
    set(CONFIG_STRING "${CONFIG_STRING}_quant_timing=${CONFIG_OPUS_ENABLE_QUANT_BANDS_TIMING}")

    # Check if we need to re-stage
    set(NEED_STAGING TRUE)
    if(EXISTS "${STAGING_MARKER}")
        file(READ "${STAGING_MARKER}" STAGED_CONFIG)
        string(STRIP "${STAGED_CONFIG}" STAGED_CONFIG)
        if("${STAGED_CONFIG}" STREQUAL "${CONFIG_STRING}")
            set(NEED_STAGING FALSE)
            message(STATUS "Opus: Using existing staged directory (up to date)")
        endif()
    endif()

    if(NEED_STAGING)
        message(STATUS "Opus: Creating staged copy of opus submodule...")

        # Remove old staging directory if it exists
        if(EXISTS "${STAGED_DIR}")
            file(REMOVE_RECURSE "${STAGED_DIR}")
        endif()

        # Copy the submodule to staging directory
        file(COPY "${SOURCE_DIR}/" DESTINATION "${STAGED_DIR}")

        # Write configuration marker
        file(WRITE "${STAGING_MARKER}" "${CONFIG_STRING}")

        message(STATUS "Opus: Staged directory created at ${STAGED_DIR}")
    endif()
endfunction()

# ==============================================================================
# opus_apply_patch
# ==============================================================================
# Applies a single patch file to the staged directory.
#
# Arguments:
#   STAGED_DIR - Path to the staged opus directory
#   PATCH_FILE - Name of the patch file (in patches/diffs/)
# ==============================================================================
function(opus_apply_patch STAGED_DIR PATCH_FILE)
    set(FULL_PATCH_PATH "${OPUS_DIFFS_DIR}/${PATCH_FILE}")

    if(NOT EXISTS "${FULL_PATCH_PATH}")
        message(FATAL_ERROR "Opus: Patch file not found: ${FULL_PATCH_PATH}")
    endif()

    # Create a marker file to track if this patch was already applied
    string(REPLACE ".patch" "" PATCH_MARKER "${PATCH_FILE}")
    set(MARKER_FILE "${STAGED_DIR}/.patch_${PATCH_MARKER}")

    if(NOT EXISTS "${MARKER_FILE}")
        # Apply the patch
        execute_process(
            COMMAND patch -p1 -i "${FULL_PATCH_PATH}"
            WORKING_DIRECTORY "${STAGED_DIR}"
            RESULT_VARIABLE PATCH_RESULT
            OUTPUT_VARIABLE PATCH_OUTPUT
            ERROR_VARIABLE PATCH_ERROR
        )

        if(NOT PATCH_RESULT EQUAL 0)
            message(FATAL_ERROR "Opus: Failed to apply patch ${PATCH_FILE}:\n${PATCH_ERROR}")
        endif()

        # Write marker
        file(WRITE "${MARKER_FILE}" "applied")
        message(STATUS "Opus: Applied patch: ${PATCH_FILE}")
    endif()
endfunction()

# ==============================================================================
# opus_apply_core_patches
# ==============================================================================
# Applies all core patches that are needed for any build.
#
# Arguments:
#   STAGED_DIR - Path to the staged opus directory
# ==============================================================================
function(opus_apply_core_patches STAGED_DIR)
    foreach(PATCH ${OPUS_CORE_PATCHES})
        opus_apply_patch("${STAGED_DIR}" "${PATCH}")
    endforeach()
endfunction()

# ==============================================================================
# opus_apply_xtensa_patches
# ==============================================================================
# Applies Xtensa LX7 optimization patches for ESP32-S3.
#
# Arguments:
#   STAGED_DIR - Path to the staged opus directory
# ==============================================================================
function(opus_apply_xtensa_patches STAGED_DIR)
    # Apply diff patches
    foreach(PATCH ${OPUS_XTENSA_PATCHES})
        opus_apply_patch("${STAGED_DIR}" "${PATCH}")
    endforeach()

    # Copy additional Xtensa files
    foreach(ADDITION ${OPUS_XTENSA_ADDITIONS})
        set(SRC_FILE "${OPUS_PATCHES_DIR}/${ADDITION}")
        set(DST_FILE "${STAGED_DIR}/${ADDITION}")

        # Create destination directory if needed
        get_filename_component(DST_DIR "${DST_FILE}" DIRECTORY)
        file(MAKE_DIRECTORY "${DST_DIR}")

        # Copy file
        configure_file("${SRC_FILE}" "${DST_FILE}" COPYONLY)
    endforeach()

    message(STATUS "Opus: Applied Xtensa LX7 optimizations")
endfunction()

# ==============================================================================
# opus_apply_timing_patches
# ==============================================================================
# Applies timing instrumentation patches to the staged directory.
# These patches add timing code to existing functions.
#
# Note: PVQ_TIMING and QUANT_BANDS_TIMING are mutually exclusive as they both
# modify vq.c. QUANT_BANDS_TIMING also modifies bands.c.
#
# Arguments:
#   STAGED_DIR         - Path to the staged opus directory
#   CELT_TIMING        - TRUE to enable CELT decoder timing
#   PVQ_TIMING         - TRUE to enable PVQ timing
#   QUANT_BANDS_TIMING - TRUE to enable quant_all_bands timing
# ==============================================================================
function(opus_apply_timing_patches STAGED_DIR CELT_TIMING PVQ_TIMING QUANT_BANDS_TIMING)
    # CELT decoder timing - patches celt_decoder.c
    if(CELT_TIMING)
        opus_apply_patch("${STAGED_DIR}" "celt_timing.patch")
    endif()

    # PVQ timing - patches vq.c
    if(PVQ_TIMING)
        opus_apply_patch("${STAGED_DIR}" "pvq_timing.patch")
    # Quant bands timing - patches vq.c (mutually exclusive with PVQ)
    elseif(QUANT_BANDS_TIMING)
        opus_apply_patch("${STAGED_DIR}" "vq_quant_bands_timing.patch")
    endif()

    # Quant bands timing - also patches bands.c
    if(QUANT_BANDS_TIMING)
        opus_apply_patch("${STAGED_DIR}" "bands_timing.patch")
    endif()
endfunction()

# ==============================================================================
# opus_setup_staged_build
# ==============================================================================
# Main entry point: creates staging directory and applies appropriate patches.
#
# Arguments:
#   COMPONENT_DIR  - Root directory of microOpus component
#   APPLY_XTENSA   - TRUE to apply Xtensa LX7 patches (for ESP32-S3)
#
# Sets in parent scope:
#   OPUS_STAGED_DIR - Path to the staged opus directory (for use in include paths)
# ==============================================================================
function(opus_setup_staged_build COMPONENT_DIR APPLY_XTENSA)
    set(SOURCE_DIR "${COMPONENT_DIR}/lib/opus")
    set(STAGED_DIR "${CMAKE_CURRENT_BINARY_DIR}/opus-staged")

    # Create staging directory
    opus_create_staging_directory("${SOURCE_DIR}" "${STAGED_DIR}" "${APPLY_XTENSA}")

    # Apply core patches
    opus_apply_core_patches("${STAGED_DIR}")

    # Apply Xtensa patches if requested
    if(APPLY_XTENSA)
        opus_apply_xtensa_patches("${STAGED_DIR}")
    endif()

    # Apply timing instrumentation patches if enabled (ESP-IDF only, uses CONFIG_ variables)
    if(CONFIG_OPUS_ENABLE_CELT_TIMING OR CONFIG_OPUS_ENABLE_PVQ_TIMING OR CONFIG_OPUS_ENABLE_QUANT_BANDS_TIMING)
        opus_apply_timing_patches(
            "${STAGED_DIR}"
            "${CONFIG_OPUS_ENABLE_CELT_TIMING}"
            "${CONFIG_OPUS_ENABLE_PVQ_TIMING}"
            "${CONFIG_OPUS_ENABLE_QUANT_BANDS_TIMING}"
        )
    endif()

    # Export the staged directory path
    set(OPUS_STAGED_DIR "${STAGED_DIR}" PARENT_SCOPE)
endfunction()
