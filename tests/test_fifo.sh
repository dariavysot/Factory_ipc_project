#!/usr/bin/env bash
set -e

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

echo "[TEST] Running FIFO defect detection integrity test with 20 items..."
make -s clean
make -s

# Run factory simulation with 20 items to guarantee defect occurrence
OUTPUT=$(./factory 20 2>&1)

# Count dispatched, inspected, and received items
DISPATCHED_COUNT=$(echo "$OUTPUT" | grep -c "\[Supervisor\] Dispatched item" || true)
INSPECTED_COUNT=$(echo "$OUTPUT" | grep -c "\[Worker 1\] Item #" || true)
# Worker 2 either tests a standard item or skips a defect
TESTED_COUNT=$(echo "$OUTPUT" | grep -c "\[Worker 2\] Testing item" || true)
SKIPPED_COUNT=$(echo "$OUTPUT" | grep -c "\[Worker 2\] Skipping item" || true)
W2_TOTAL=$((TESTED_COUNT + SKIPPED_COUNT))
DEFECT_COUNT=$(echo "$OUTPUT" | grep -c "DEFECT" || true)

echo "  -> Dispatched by Supervisor: $DISPATCHED_COUNT"
echo "  -> Inspected by Worker 1:   $INSPECTED_COUNT"
echo "  -> Handled by Worker 2:     $W2_TOTAL (Tested: $TESTED_COUNT, Skipped defects: $SKIPPED_COUNT)"
echo "  -> Defect mentions:         $DEFECT_COUNT"

if [ "$DISPATCHED_COUNT" -eq 20 ] && [ "$INSPECTED_COUNT" -eq 20 ] && [ "$W2_TOTAL" -eq 20 ]; then
    echo "[SUCCESS] FIFO test passed: All items transferred through unnamed and named pipes correctly."
    exit 0
else
    echo "[FAILURE] FIFO test failed: Item count mismatch across the pipeline."
    exit 1
fi
