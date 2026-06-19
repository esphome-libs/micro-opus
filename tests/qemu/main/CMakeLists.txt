# Generate vectors_data.c from tests/vectors/*.bit at configure time, then build
# it into the firmware alongside the test driver. embed_vectors.py always emits a
# valid (possibly empty) table, so the build never fails just because the vectors
# have not been fetched yet: the firmware reports an empty run and the runner
# treats that as "skipped".
set(VECTORS_DATA "${CMAKE_CURRENT_BINARY_DIR}/vectors_data.c")
set(VECTORS_DIR "${CMAKE_CURRENT_LIST_DIR}/../../vectors")

# IDF sets PYTHON; fall back to a plain python3 for stray PlatformIO setups.
set(EMBED_PYTHON "${PYTHON}")
if(NOT EMBED_PYTHON)
    set(EMBED_PYTHON "python3")
endif()

execute_process(
    COMMAND "${EMBED_PYTHON}" "${CMAKE_CURRENT_LIST_DIR}/embed_vectors.py"
            "${VECTORS_DIR}" "${VECTORS_DATA}"
    RESULT_VARIABLE embed_rc)
if(NOT embed_rc EQUAL 0)
    message(FATAL_ERROR "embed_vectors.py failed (rc=${embed_rc})")
endif()

idf_component_register(
    SRCS "qemu_opus_test.cpp" "${VECTORS_DATA}"
    INCLUDE_DIRS "."
    REQUIRES micro-opus esp_driver_uart)
