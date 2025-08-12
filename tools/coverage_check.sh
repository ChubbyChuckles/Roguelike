#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build}"
INFO_FILE="${BUILD_DIR}/coverage.info"

if ! command -v lcov >/dev/null 2>&1; then
  echo "lcov not installed" >&2
  exit 1
fi

lcov --directory "${BUILD_DIR}" --capture --output-file "${INFO_FILE}" 1>/dev/null
lcov --remove "${INFO_FILE}" '/usr/*' '*/tests/*' '*/_deps/*' '*/src/main.c' --output-file "${INFO_FILE}" 1>/dev/null

echo "Coverage summary:"
lcov --list "${INFO_FILE}" | tail -n 25

PERCENT=$(lcov --list "${INFO_FILE}" | awk '/lines......:/ { pct=$2 } END { gsub("%","",pct); print pct }')
if [ -z "${PERCENT}" ]; then
  echo "Could not determine coverage percentage" >&2
  exit 1
fi

PERCENT_INT=$(printf '%.0f' "${PERCENT}")
if [ "${PERCENT_INT}" -lt 100 ]; then
  echo "FAIL: Coverage is ${PERCENT}% (< 100%). Add tests to cover remaining lines." >&2
  exit 1
fi
echo "PASS: Coverage is 100%."
