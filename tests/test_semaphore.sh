#!/bin/bash
set -e

echo "[TEST] Running Semaphore dual-condition break test..."
make -s clean
rm -f break_log.txt
make -s

# Run factory simulation with 20 items to allow both triggers to fire
./factory 20 > /dev/null 2>&1

if [ ! -f break_log.txt ]; then
    echo "[FAILURE] break_log.txt was not created."
    exit 1
fi

START_COUNT=$(grep -c "START break" break_log.txt || true)
END_COUNT=$(grep -c "END   break" break_log.txt || true)
BATCH_TRIGGERS=$(grep -c "Batch quota" break_log.txt || true)
TIME_TRIGGERS=$(grep -c "Time limit" break_log.txt || true)

echo "  -> Total break START entries: $START_COUNT"
echo "  -> Total break END   entries: $END_COUNT"
echo "  -> Batch quota triggers:     $BATCH_TRIGGERS"
echo "  -> Time limit triggers:      $TIME_TRIGGERS"

if [ "$START_COUNT" -ge 2 ] && [ "$START_COUNT" -eq "$END_COUNT" ]; then
    echo "[SUCCESS] Semaphore test passed: Mutex upheld, start/end balance maintained."
    exit 0
else
    echo "[FAILURE] Semaphore test failed: Mismatched entries in break log."
    exit 1
fi
