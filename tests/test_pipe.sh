#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

echo "[TEST] Running unnamed pipe integrity test with 10 items..."
make -s clean
make -s

# Run factory simulation and capture combined stdout/stderr
OUTPUT=$(./factory 10 2>&1)

DISPATCHED_COUNT=$(echo "$OUTPUT" | grep -c "\[Supervisor\] Dispatched item" || true)
INSPECTED_COUNT=$(echo "$OUTPUT" | grep -c "\[Worker 1\] Item #" || true)

echo "  -> Items dispatched by Supervisor: $DISPATCHED_COUNT"
echo "  -> Items processed by Worker 1:    $INSPECTED_COUNT"

if [ "$DISPATCHED_COUNT" -eq 10 ] && [ "$INSPECTED_COUNT" -eq 10 ]; then
    echo "[SUCCESS] Pipe test passed: All items transferred without loss."
    exit 0
else
    echo "[FAILURE] Pipe test failed: Mismatch in dispatched vs inspected items."
    exit 1
fi
