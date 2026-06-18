cmake_minimum_required(VERSION 3.16)
project(micro_opus_tests C CXX)

# ==============================================================================
# Host test suite for microOpus
#
# Builds the microOpus host library (fixed-point) plus a set of CTest-driven tests:
#   unit/        - focused tests of our own wrapper and parsing code
#   conformance/ - opus_compare-based validation of our patched libopus
#
# Run with:
#   cmake -B build tests
#   cmake --build build
#   ctest --test-dir build --output-on-failure
#
# For the RFC test-vector conformance test, first download the vectors:
#   tests/fetch_vectors.sh
# ==============================================================================

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 99)

# Default to an optimized build when the caller hasn't picked a type and sanitizers are off.
# Without this the test executables compile at -O0; opus_compare's per-frame FFT then dominates the
# conformance run over the ~640 s of reference audio and is several times slower. (The Opus library
# itself is always -O2 via Opus's own build system, which otherwise hides this.) Sanitizer builds
# are left unoptimized on purpose so UBSan/ASan see un-elided code.
if(NOT CMAKE_BUILD_TYPE AND NOT ENABLE_SANITIZERS)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    message(STATUS "Tests: defaulting CMAKE_BUILD_TYPE to Release")
endif()

# Sanitizers (recommended for development; CI runs the suite with them on).
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(ENABLE_SANITIZERS)
    message(STATUS "Tests: building with AddressSanitizer + UndefinedBehaviorSanitizer")
    set(_san_flags "-fsanitize=address,undefined -fno-omit-frame-pointer -g")
    string(APPEND CMAKE_C_FLAGS " ${_san_flags}")
    string(APPEND CMAKE_CXX_FLAGS " ${_san_flags}")
    string(APPEND CMAKE_EXE_LINKER_FLAGS " ${_san_flags}")
endif()

set(MICRO_OPUS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Build the microOpus library (and its Ogg demuxer dependency) as a host static library. Flags set
# above propagate into these so the library is instrumented too.
if(NOT TARGET micro_ogg_demuxer)
    add_subdirectory(${MICRO_OPUS_ROOT}/lib/micro-ogg-demuxer
                     ${CMAKE_CURRENT_BINARY_DIR}/micro-ogg-demuxer)
endif()
# The root CMakeLists declares project(micro_opus); CMake supports this nested project() call.
add_subdirectory(${MICRO_OPUS_ROOT} ${CMAKE_CURRENT_BINARY_DIR}/micro-opus)

enable_testing()

# Shared include paths for tests: the in-memory Ogg muxing helpers, and src/ for internal headers
# (opus_header.h) that the library compiles but does not export publicly.
add_library(test_support INTERFACE)
target_include_directories(test_support INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/support
    ${MICRO_OPUS_ROOT}/src
)

# ==============================================================================
# Unit tests - exercise our own wrapper / parsing code
# ==============================================================================

function(micro_opus_add_unit_test name)
    add_executable(${name} unit/${name}.cpp)
    target_link_libraries(${name} PRIVATE micro_opus test_support)
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS unit)
endfunction()

micro_opus_add_unit_test(test_opus_header)       # RFC 7845 OpusHead/OpusTags parsing
micro_opus_add_unit_test(test_raw_packet)        # OpusPacketDecoder round-trip + error paths
micro_opus_add_unit_test(test_silent_channels)   # OggOpusDecoder channel mapping family 1 (255)
micro_opus_add_unit_test(test_chunked)           # OggOpusDecoder 64-byte chunked buffering

# ==============================================================================
# Conformance tests - opus_compare validation of our patched libopus
#
# These use shell drivers, so they are gated to non-Windows hosts. The unit tests above run
# everywhere.
# ==============================================================================

if(NOT WIN32)
    # opus_compare: pristine standalone comparator from upstream (no opus library dependency).
    add_executable(opus_compare ${MICRO_OPUS_ROOT}/lib/opus/src/opus_compare.c)
    target_link_libraries(opus_compare PRIVATE m)

    # Our test-vector decoder (decodes the .bit framing with OpusPacketDecoder).
    add_executable(decode_vectors conformance/decode_vectors.cpp)
    target_link_libraries(decode_vectors PRIVATE micro_opus)

    # RFC 8251 test-vector conformance. Skips (CTest "Skipped") until the vectors are
    # downloaded into OPUS_VECTOR_DIR via tests/fetch_vectors.sh.
    set(OPUS_VECTOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vectors
        CACHE PATH "Directory holding testvector*.bit/.dec files")
    add_test(NAME conformance_vectors
        COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/conformance/run_vectors.sh
                $<TARGET_FILE:decode_vectors> $<TARGET_FILE:opus_compare>
                ${OPUS_VECTOR_DIR} ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(conformance_vectors PROPERTIES SKIP_RETURN_CODE 77 LABELS conformance)
endif()

# ==============================================================================
# Measurement tools (not tests) - opt in with -DBUILD_MEASURE_TOOLS=ON
#
# measure_zerocopy needs the Ogg demuxer's debug stats, which requires building the library with
# MICRO_OGG_DEMUXER_DEBUG, so it is kept out of the default build.
# ==============================================================================

option(BUILD_MEASURE_TOOLS "Build measurement tools (rebuilds the library with debug stats)" OFF)
if(BUILD_MEASURE_TOOLS AND NOT WIN32)
    # MICRO_OGG_DEMUXER_DEBUG must be PUBLIC because the zero-copy stats it gates live inside the
    # micro_opus / micro_ogg_demuxer libraries, not in the tool. The trade-off is that it also
    # instruments the library the unit/conformance tests link in THIS build, so prefer a dedicated
    # build directory for the tool rather than mixing it with a canonical test run.
    message(WARNING
        "BUILD_MEASURE_TOOLS rebuilds micro_opus/micro_ogg_demuxer with MICRO_OGG_DEMUXER_DEBUG; "
        "use a separate build dir for measurement tools so the test suite runs against the "
        "uninstrumented library.")
    add_executable(measure_zerocopy tools/measure_zerocopy.cpp)
    target_link_libraries(measure_zerocopy PRIVATE micro_opus)
    target_compile_definitions(measure_zerocopy PRIVATE MICRO_OGG_DEMUXER_DEBUG)
    target_compile_definitions(micro_opus PUBLIC MICRO_OGG_DEMUXER_DEBUG)
    target_compile_definitions(micro_ogg_demuxer PUBLIC MICRO_OGG_DEMUXER_DEBUG)
endif()
