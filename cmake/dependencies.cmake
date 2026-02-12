# cmake/dependencies.cmake
# Dependency resolution for microOpus
#
# When used as an ESP-IDF managed component, git submodules are not available.
# This module uses FetchContent to download dependencies when the submodule
# directories don't exist.

# Guard against multiple inclusion
if(__opus_dependencies_defined)
    return()
endif()
set(__opus_dependencies_defined TRUE)

include(FetchContent)

# Pinned dependency versions (must match .gitmodules)
set(OPUS_VERSION "v1.6.1")
set(MICRO_OGG_DEMUXER_VERSION "v1.1.0")

# ==============================================================================
# Resolve opus library source directory
# ==============================================================================
set(OPUS_LIB_DIR "${OPUS_COMPONENT_DIR}/lib/opus")

if(NOT EXISTS "${OPUS_LIB_DIR}/include/opus.h")
    message(STATUS "micro-opus: lib/opus submodule not found, fetching opus ${OPUS_VERSION}...")
    FetchContent_Declare(
        opus_source
        URL "https://github.com/xiph/opus/archive/refs/tags/${OPUS_VERSION}.tar.gz"
    )
    FetchContent_GetProperties(opus_source)
    if(NOT opus_source_POPULATED)
        FetchContent_Populate(opus_source)
    endif()
    set(OPUS_LIB_DIR "${opus_source_SOURCE_DIR}")
    message(STATUS "micro-opus: Fetched opus to ${OPUS_LIB_DIR}")
endif()

# ==============================================================================
# Resolve micro-ogg-demuxer source directory
# ==============================================================================
set(MICRO_OGG_LIB_DIR "${OPUS_COMPONENT_DIR}/lib/micro-ogg-demuxer")

if(NOT EXISTS "${MICRO_OGG_LIB_DIR}/CMakeLists.txt")
    message(STATUS "micro-opus: lib/micro-ogg-demuxer submodule not found, fetching ${MICRO_OGG_DEMUXER_VERSION}...")
    FetchContent_Declare(
        micro_ogg_source
        URL "https://github.com/esphome-libs/micro-ogg-demuxer/archive/refs/tags/${MICRO_OGG_DEMUXER_VERSION}.tar.gz"
    )
    FetchContent_GetProperties(micro_ogg_source)
    if(NOT micro_ogg_source_POPULATED)
        FetchContent_Populate(micro_ogg_source)
    endif()
    set(MICRO_OGG_LIB_DIR "${micro_ogg_source_SOURCE_DIR}")
    message(STATUS "micro-opus: Fetched micro-ogg-demuxer to ${MICRO_OGG_LIB_DIR}")
endif()
