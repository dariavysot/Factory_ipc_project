#!/bin/bash
set -e

echo "[TEST] Running Semaphore break exclusion and file logging test..."
make -s clean
rm -f break_log.txt
make -s

# Run factory simulation
./factory 8 > /dev/null 2>&1

# Verify log file exists
if [ ! -f break_log.txt ]; then
    echo "[FAILURE] break_log.txt was not created."
    exit 1
fi

# Count start and end records
START_COUNT=$(grep -c "START break" break_log.txt || true)
END_COUNT=$(grep -c "END   break" break_log.txt || true)

echo "  -> Break START entries: $START_COUNT"
echo "  -> Break END   entries: $END_COUNT"

if [ "$START_COUNT" -ge 2 ] && [ "$START_COUNT" -eq "$END_COUNT" ]; then
    echo "[SUCCESS] Semaphore test passed: Both workers logged breaks without leaving broken states."
    exit 0
else
    echo "[FAILURE] Semaphore test failed: Mismatch in start/end break log entries."
    exit 1
fi
