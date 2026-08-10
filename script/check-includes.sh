#!/bin/bash

# Include-what-you-use check via clang-include-cleaner
#
# Verifies every source/header directly includes what it uses, with one
# relaxation: a foo.cpp may rely on headers its matching foo.h already
# includes. ESP-IDF branches are checked too, using stub headers
# materialized from script/esp_stubs.py. See script/check_includes.py
# for the full design.
#
# Usage: ./script/check-includes.sh [--fix]

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

exec python3 -B "${SCRIPT_DIR}/check_includes.py" "$@"
