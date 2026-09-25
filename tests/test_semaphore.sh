#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

echo "[TEST] Building project..."
make clean > /dev/null
make > /dev/null

LOG_FILE="break_log.txt"
rm -f "$LOG_FILE"

TOTAL_ITEMS=20
echo "[TEST] Running factory with $TOTAL_ITEMS items to verify break logic..."
./factory "$TOTAL_ITEMS" > /dev/null

if [ ! -f "$LOG_FILE" ]; then
    echo "[FAIL] Log file $LOG_FILE was not created!"
    exit 1
fi

START_COUNT=$(grep -c "START break" "$LOG_FILE" || true)
END_COUNT=$(grep -c "END   break" "$LOG_FILE" || true)

echo "  -> Total break START entries: $START_COUNT"
echo "  -> Total break END   entries: $END_COUNT"

# 1. Check mutual exclusion balance
if [ "$START_COUNT" -eq 0 ]; then
    echo "[FAIL] No breaks were recorded for $TOTAL_ITEMS items!"
    exit 1
fi

if [ "$START_COUNT" -ne "$END_COUNT" ]; then
    echo "[FAIL] Mismatch between START ($START_COUNT) and END ($END_COUNT) breaks!"
    exit 1
fi

# 2. Check break frequency sanity (avoid duplicate counting bug)
# For 20 items and threshold 5-7, each worker should take at most 4 breaks (max 8 total).
MAX_EXPECTED_BREAKS=8
if [ "$START_COUNT" -gt "$MAX_EXPECTED_BREAKS" ]; then
    echo "[FAIL] Workers took too many breaks ($START_COUNT > $MAX_EXPECTED_BREAKS) for $TOTAL_ITEMS items! Double counting detected."
    exit 1
fi

# 3. Check chronological ordering (no overlapping breaks in log)
PREV_END=""
OVERLAP_DETECTED=0

while IFS= read -r line; do
    if [[ "$line" =~ START.*at\ ([0-9]{4}-[0-9]{2}-[0-9]{2}\ [0-9]{2}:[0-9]{2}:[0-9]{2}) ]]; then
        START_TIME="${BASH_REMATCH[1]}"
        if [ -n "$PREV_END" ] && [[ "$START_TIME" < "$PREV_END" ]]; then
            echo "[FAIL] Overlapping break detected! Start: $START_TIME before previous End: $PREV_END"
            OVERLAP_DETECTED=1
            break
        fi
    elif [[ "$line" =~ END.*at\ ([0-9]{4}-[0-9]{2}-[0-9]{2}\ [0-9]{2}:[0-9]{2}:[0-9]{2}) ]]; then
        PREV_END="${BASH_REMATCH[1]}"
    fi
done < "$LOG_FILE"

if [ "$OVERLAP_DETECTED" -eq 1 ]; then
    exit 1
fi

echo "[SUCCESS] Break & Semaphore tests passed:"
echo "  - START and END balanced"
echo "  - Break frequency is sane (no double-counting: $START_COUNT <= $MAX_EXPECTED_BREAKS)"
echo "  - No overlapping critical sections (mutual exclusion strictly held)"
exit 0