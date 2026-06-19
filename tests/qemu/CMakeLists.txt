# Top-level ESP-IDF project for the on-target (QEMU) conformance firmware.
#
# Built with idf.py in CI (espressif/idf container) and with PlatformIO locally;
# both point at main/ as the app component. The micro-opus component is pulled
# in from the repo root via EXTRA_COMPONENT_DIRS.
cmake_minimum_required(VERSION 3.16)

list(APPEND EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../..")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(qemu_opus_test)
