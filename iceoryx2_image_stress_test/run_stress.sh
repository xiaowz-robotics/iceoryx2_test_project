#!/bin/bash
ROUNDS=${1:-100}
INTERVAL_MS=${2:-5}
TIMEOUT_SEC=${3:-120}
LOG_DIR="./logs_$(date +%Y%m%d_%H%M%S)"

mkdir -p "$LOG_DIR"
echo "Logs: $LOG_DIR"

RECEIVER_PIDS=()
SENDER_PIDS=()

echo "Starting 10 receivers..."
for i in $(seq 1 10); do
    ./build/image_receiver "$i" "$ROUNDS" "$TIMEOUT_SEC" 2>&1 | tee "$LOG_DIR/receiver_${i}.log" &
    RECEIVER_PIDS+=($!)
done

sleep 2

echo "Starting 10 senders..."
for i in $(seq 1 10); do
    ./build/image_sender "$i" "$ROUNDS" "$INTERVAL_MS" 2>&1 | tee "$LOG_DIR/sender_${i}.log" &
    SENDER_PIDS+=($!)
done

echo "Waiting for senders..."
for pid in "${SENDER_PIDS[@]}"; do
    wait "$pid"
done

echo "Waiting for receivers..."
for pid in "${RECEIVER_PIDS[@]}"; do
    wait "$pid"
done

echo ""
echo "================ Aggregate Summary ================"
TOTAL_EXPECTED=$((10 * ROUNDS))
TOTAL_RECV=0
for i in $(seq 1 10); do
    RES=$(grep "SUMMARY_RECEIVER_${i}:" "$LOG_DIR/receiver_${i}.log" | cut -d: -f2)
    RECV=$(echo "$RES" | cut -d/ -f1)
    echo "Receiver $i: $RES"
    TOTAL_RECV=$((TOTAL_RECV + RECV))
done
echo "Aggregate received: $TOTAL_RECV/$TOTAL_EXPECTED"
echo "Logs saved in: $LOG_DIR"
echo "==================================================="
