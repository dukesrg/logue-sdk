#!/bin/bash
case "$1" in
    "nts-1_mkii")
        echo "Testing NTS-1 mkII parameters..."
        PLATFORM=$1
        PLATFORM_ID=73
        SEND_DEVICE="NTS-1 digital kit mkII NTS-1 digital kit _ SOUND"
        RECEIVE_DEVICE="NTS-1 digital kit mkII NTS-1 digital kit _ KBD/KNOB"
        UNIT="osc"
        PARAM_MSB=0
        PARAM_LSB_MULTIPLIER=1
        PARAM_LSB_OFFSET=0
        ;;
    "nts-3_kaoss")
        echo "Testing NTS-3 parameters..."
        PLATFORM=$1
        PLATFORM_ID=72
        SEND_DEVICE="NTS-3 kaoss pad kit SOUND"
        RECEIVE_DEVICE="NTS-3 kaoss pad kit XY/KNOB"
        UNIT="genericfx"
        PARAM_MSB=1
        PARAM_LSB_MULTIPLIER=8
        PARAM_LSB_OFFSET=1
        ;;
    *)
        echo "Usage: $0 {nts-1_mkii|nts-3_kaoss}"
        exit 1
        ;;
esac

echo "7-bit LSB first..."
echo "====="
PARAM_INDEX=0
PARAM_LSB=$((PARAM_INDEX * PARAM_LSB_MULTIPLIER + PARAM_LSB_OFFSET))
receivemidi dev "${RECEIVE_DEVICE}" syx jsf $PLATFORM-$UNIT-$PARAM_INDEX.js q &
RECEIVE_PID=$!
echo "LSB, MSB, Raw, RcvSeq, RcvLSB, RcvMSB, RcvRaw" 
LSB=0
for MSB in {0..127}; do
    echo -n "$LSB, $MSB, $MSB, " 
    sendmidi dev "${SEND_DEVICE}" cc 99 $PARAM_MSB cc 98 $PARAM_LSB cc 6 $MSB cc 38 $LSB syx hex 42 30 00 01 $PLATFORM_ID 10
    sleep 0.05
done
kill $RECEIVE_PID
echo "====="

echo "10-bit LSB first..."
echo "====="
PARAM_INDEX=1
PARAM_LSB=$((PARAM_INDEX * PARAM_LSB_MULTIPLIER + PARAM_LSB_OFFSET))
receivemidi dev "${RECEIVE_DEVICE}" syx jsf $PLATFORM-$UNIT-$PARAM_INDEX.js q &
RECEIVE_PID=$!
echo "LSB, MSB, Raw, RcvSeq, RcvLSB, RcvMSB, RcvRaw" 
for MSB in {0..127}; do 
for LSB in 0 16 32 48 64 80 96 112; do 
    echo -n "$LSB, $MSB, $((LSB / 16 + MSB * 8)), " 
    sendmidi dev "${SEND_DEVICE}" cc 99 $PARAM_MSB cc 98 $PARAM_LSB cc 6 $MSB cc 38 $LSB syx hex 42 30 00 01 $PLATFORM_ID 10
    sleep 0.05
done
done
kill $RECEIVE_PID
echo "====="

echo "14-bit LSB first..."
echo "====="
PARAM_INDEX=2
PARAM_LSB=$((PARAM_INDEX * PARAM_LSB_MULTIPLIER + PARAM_LSB_OFFSET))
receivemidi dev "${RECEIVE_DEVICE}" syx jsf $PLATFORM-$UNIT-$PARAM_INDEX.js q &
RECEIVE_PID=$!
echo "LSB, MSB, Raw, RcvSeq, RcvLSB, RcvMSB, RcvRaw" 
for MSB in {0..127}; do 
for LSB in {0..127}; do 
    echo -n "$LSB, $MSB, $((LSB + MSB * 128)), " 
    sendmidi dev "${SEND_DEVICE}" cc 99 $PARAM_MSB cc 98 $PARAM_LSB cc 6 $MSB cc 38 $LSB syx hex 42 30 00 01 $PLATFORM_ID 10
    sleep 0.05
done
done
kill $RECEIVE_PID 2>/dev/null
echo "====="
