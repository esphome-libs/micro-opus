# cmake/functions.cmake
# Helper functions for microOpus build system

# Guard against multiple inclusion
if(__opus_functions_defined)
    return()
endif()
set(__opus_functions_defined TRUE)

# ==============================================================================
# opus_set_common_definitions
# ==============================================================================
# Sets compile definitions common to all build configurations.
#
# Arguments:
#   TARGET - The target to apply definitions to
# ==============================================================================
function(opus_set_common_definitions TARGET)
    target_compile_definitions(${TARGET} PRIVATE
        HAVE_CONFIG_H
        OPUS_BUILD
        OPUS_EXPORT=
        OPUS_HAVE_RTCD=0
        HAVE_LRINT
        HAVE_LRINTF
        CUSTOM_SUPPORT
    )
endfunction()

# ==============================================================================
# opus_set_optimization_flags
# ==============================================================================
# Sets common optimization compiler flags.
#
# Arguments:
#   TARGET - The target to apply flags to
# ==============================================================================
function(opus_set_optimization_flags TARGET)
    target_compile_options(${TARGET} PRIVATE
        -O2
        -ffunction-sections
        -fdata-sections
    )
endfunction()
