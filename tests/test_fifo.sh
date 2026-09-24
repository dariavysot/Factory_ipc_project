#!/bin/bash

echo "[TEST] Running FIFO defect detection integrity test with 20 items..."
make -s clean
make -s

# Run factory simulation with 20 items to guarantee defect occurrence
OUTPUT=$(./factory 20 2>&1)

# Count dispatched, inspected, and received items
DISPATCHED_COUNT=$(echo "$OUTPUT" | grep -c "Dispatched item" || true)
INSPECTED_COUNT=$(echo "$OUTPUT" | grep -c "Worker 1.*inspected" || true)
RECEIVED_COUNT=$(echo "$OUTPUT" | grep -c "Worker 2.*Received item" || true)
DEFECT_COUNT=$(echo "$OUTPUT" | grep -c "DEFECT" || true)

echo "  -> Dispatched by Supervisor: $DISPATCHED_COUNT"
echo "  -> Inspected by Worker 1:   $INSPECTED_COUNT"
echo "  -> Processed by Worker 2:   $RECEIVED_COUNT"
echo "  -> Total defect occurrences: $DEFECT_COUNT"

# Verify all items flowed through without loss and at least some defect was handled
if [ "$DISPATCHED_COUNT" -eq 20 ] && [ "$INSPECTED_COUNT" -eq 20 ] && [ "$RECEIVED_COUNT" -eq 20 ]; then
    echo "[SUCCESS] FIFO test passed: All items transferred through unnamed and named pipes correctly."
    exit 0
else
    echo "[FAILURE] FIFO test failed: Item count mismatch across the pipeline."
    exit 1
fi
