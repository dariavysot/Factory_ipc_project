#!/bin/bash

echo "[TEST] Running Message Queue quality pipeline test with 25 items..."
make -s clean
make -s

OUTPUT=$(./factory 25 2>&1)

# Specifically count items dispatched by Supervisor only
DISPATCHED=$(echo "$OUTPUT" | grep -c "\[Supervisor\] Dispatched item" || true)
MQ_SENT=$(echo "$OUTPUT" | grep -c "\[Worker 2\] Dispatched item.*to Message Queue" || true)
MQ_VERIFIED=$(echo "$OUTPUT" | grep -c "Verified item #" || true)
QUEUE_CLEANED=$(echo "$OUTPUT" | grep -c "Message queue deallocated successfully" || true)

echo "  -> Dispatched by Supervisor:      $DISPATCHED"
echo "  -> Sent to MQ by Worker 2:         $MQ_SENT"
echo "  -> Verified by Supervisor from MQ: $MQ_VERIFIED"
echo "  -> Queue cleaned:                  $QUEUE_CLEANED"

if [ "$DISPATCHED" -eq 25 ] && [ "$MQ_SENT" -eq "$MQ_VERIFIED" ] && [ "$MQ_SENT" -gt 0 ] && [ "$QUEUE_CLEANED" -eq 1 ]; then
    echo "[SUCCESS] Message queue test passed: All standard items verified and resources cleaned up."
    exit 0
else
    echo "[FAILURE] Message queue test failed: Count mismatch or queue cleanup failure."
    exit 1
fi
