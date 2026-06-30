#!/usr/bin/env bash
set -euo pipefail

# ROS 2 image stress test runner
#
# Usage:
#   ./run_ros2_stress.sh <rounds> <interval_ms> <timeout_sec> [reliable]
#
# Example:
#   ./run_ros2_stress.sh 1000 5 120 true
#
# Meaning:
#   rounds       : each publisher sends this many images
#   interval_ms  : sleep interval between frames
#   timeout_sec  : subscriber timeout
#   reliable     : true/false, default true

ROUNDS=${1:-100}
INTERVAL_MS=${2:-5}
TIMEOUT_SEC=${3:-120}
RELIABLE=${4:-true}

PKG=ros2_image_stress_test

BASE_DIR=$(cd "$(dirname "$0")" && pwd)
LOG_DIR="${BASE_DIR}/logs_ros2_$(date +%Y%m%d_%H%M%S)"
mkdir -p "${LOG_DIR}"

export RCUTILS_LOGGING_BUFFERED_STREAM=1
export RCUTILS_COLORIZED_OUTPUT=0

# 如果你只想在本机测试，不希望 ROS2 尝试外部网络发现，可以打开：
# export ROS_LOCALHOST_ONLY=1

echo "Logs: ${LOG_DIR}"
echo "ROUNDS=${ROUNDS}, INTERVAL_MS=${INTERVAL_MS}, TIMEOUT_SEC=${TIMEOUT_SEC}, RELIABLE=${RELIABLE}"

PIDS=()

cleanup() {
    echo "Cleaning up background ROS2 stress processes..."
    for pid in "${PIDS[@]:-}"; do
        if kill -0 "$pid" 2>/dev/null; then
            kill "$pid" 2>/dev/null || true
        fi
    done
}

trap cleanup INT TERM

echo "Starting 10 ROS2 subscriber nodes..."
for i in $(seq 1 10); do
    (
        ros2 run ${PKG} ros_image_subscriber --ros-args \
            -p receiver_id:=${i} \
            -p expected_count:=${ROUNDS} \
            -p timeout_sec:=${TIMEOUT_SEC} \
            -p reliable:=${RELIABLE} \
            2>&1 | tee "${LOG_DIR}/subscriber_${i}.log"
    ) &
    PIDS+=("$!")
done

sleep 3

echo "Starting 10 ROS2 publisher nodes..."
for i in $(seq 1 10); do
    (
        ros2 run ${PKG} ros_image_publisher --ros-args \
            -p sender_id:=${i} \
            -p rounds:=${ROUNDS} \
            -p interval_ms:=${INTERVAL_MS} \
            -p reliable:=${RELIABLE} \
            2>&1 | tee "${LOG_DIR}/publisher_${i}.log"
    ) &
    PIDS+=("$!")
done

echo "Waiting for all ROS2 nodes..."

set +e
for pid in "${PIDS[@]}"; do
    wait "$pid"
done
set -e

TOTAL=0
EXPECTED_TOTAL=$((ROUNDS * 10))

echo
echo "================ ROS2 Aggregate Summary ================"
for i in $(seq 1 10); do
    logfile="${LOG_DIR}/subscriber_${i}.log"

    if [[ -f "$logfile" ]]; then
        summary_count=$(grep -o "received=[0-9]*/${ROUNDS}" "$logfile" \
            | tail -n 1 \
            | sed -E 's/received=([0-9]+).*/\1/' || true)

        if [[ -z "${summary_count}" ]]; then
            summary_count=$(grep -c "\[RECV\]" "$logfile" || true)
        fi
    else
        summary_count=0
    fi

    echo "Subscriber ${i}: ${summary_count}/${ROUNDS}"
    TOTAL=$((TOTAL + summary_count))
done

echo "Aggregate received: ${TOTAL}/${EXPECTED_TOTAL}"
echo "Logs saved in: ${LOG_DIR}"
echo "========================================================"