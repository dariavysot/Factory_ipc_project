#!/bin/bash

echo "[TEST] Running unnamed pipe integrity test with 10 items..."
make -s clean
make -s

# Run factory simulation and capture combined stdout/stderr
OUTPUT=$(./factory 10 2>&1)

# Count dispatched and received items
DISPATCHED_COUNT=$(echo "$OUTPUT" | grep -c "Dispatched item" || true)
RECEIVED_COUNT=$(echo "$OUTPUT" | grep -c "received from pipe" || true)

echo "  -> Items dispatched by Supervisor: $DISPATCHED_COUNT"
echo "  -> Items received by Worker 1:      $RECEIVED_COUNT"

if [ "$DISPATCHED_COUNT" -eq 10 ] && [ "$RECEIVED_COUNT" -eq 10 ]; then
    echo "[SUCCESS] Pipe test passed: All items transferred without loss."
    exit 0
else
    echo "[FAILURE] Pipe test failed: Mismatch in dispatched vs received items."
    exit 1
fi
