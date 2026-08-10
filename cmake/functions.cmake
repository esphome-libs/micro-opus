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
# opus_wrapper_warning_flags
# ==============================================================================
# Returns (in OUT_VAR, in the caller's scope) the strict warning set applied to
# our own first-party C++ wrapper sources (src/*.cpp) on BOTH the host and the
# ESP-IDF component build. The bundled upstream Opus C is never put through these
# (it is not clean under them by design), so callers scope the result to the
# wrapper sources via set_source_files_properties().
#
# -Werror is deliberately NOT included here: each caller (host and ESP) appends it
# under the ENABLE_WERROR guard, which defaults off. These sources are
# source-distributed and consumers compile them with arbitrary future toolchains,
# so with the guard off they get warnings only; CI's pinned host and ESP builds
# pass -DENABLE_WERROR=ON, which is where the warnings actually become errors.
#
# Arguments:
#   OUT_VAR - Name of the variable to populate in the caller's scope
# ==============================================================================
function(opus_wrapper_warning_flags OUT_VAR)
    set(${OUT_VAR}
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wdouble-promotion
        -Wformat=2 -Wimplicit-fallthrough
        # Any function not declared in a header must be static, so -Wunused-function can see it go
        # dead. Clang and GCC spell the C++ variant differently.
        $<$<CXX_COMPILER_ID:Clang,AppleClang>:-Wmissing-prototypes>
        $<$<CXX_COMPILER_ID:GNU>:-Wmissing-declarations>
        # Require static_cast/reinterpret_cast over C-style casts (the wrapper sources are all C++).
        $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>
        PARENT_SCOPE)
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
    # GCC 14+ emits a false-positive -Wmaybe-uninitialized in upstream silk/NLSF2A.c: cos_LSF_QA[]
    # is filled via a permutation table the analyzer can't reason about. Demote it to non-fatal so
    # consumers building with -Werror=all (e.g. ESP-IDF defaults) don't break. GCC-only: Clang has
    # no such warning and would reject the flag as an unknown-warning-option under -Werror. Remove
    # once upstream xiph/opus silences this.
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${TARGET} PRIVATE -Wno-error=maybe-uninitialized)
    endif()
endfunction()
