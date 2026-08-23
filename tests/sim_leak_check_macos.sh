#!/bin/sh
# Leak-checks obc_sim/adcs_sim directly, live, while they're actually
# running and exchanging traffic -- deliberately NOT via
# leak_check_macos.sh's `leaks --atExit`, because that requires being the
# wrapped process's direct parent, and (see tests/CMakeLists.txt) that
# corrupts fork()-based tests. obc_sim/adcs_sim don't fork -- FreeRTOS's
# POSIX port (rtos/ports/posix/port.c) creates each task as a real
# pthread, not a forked process -- so attaching `leaks <pid>` to them
# live, while they're still running, sidesteps that problem entirely.
# Verified by hand before wiring this in.
#
# MallocStackLogging=1 is required in the target's environment or `leaks`
# silently reports 0 leaks even when there are real ones (verified: a
# deliberately leaked malloc() was missed without it, found with it).
#
# Usage: sim_leak_check_macos.sh <obc_sim_path> <adcs_sim_path>

set -e

OBC_SIM="$1"
ADCS_SIM="$2"
RUN_SEC=8

if [ -z "$OBC_SIM" ] || [ -z "$ADCS_SIM" ]; then
    echo "usage: $0 <obc_sim_path> <adcs_sim_path>" >&2
    exit 1
fi

# Same hardcoded socket path as production code -- don't run this
# alongside a manually-launched obc_sim/adcs_sim or the other tests.
rm -f /tmp/comms_i2c.sock

export OBC_POSITION_PERIOD_SEC=2   # fast cadence so real command/ACK/telemetry traffic happens inside RUN_SEC

# MallocStackLogging=1 only needs to be set on the target processes
# themselves (it's a launch-time property `leaks` reads back later) -- kept
# scoped to just these two commands so it doesn't leak into `leaks`/`grep`
# below and spam their own MallocStackLogging diagnostics into the output.
MallocStackLogging=1 "$OBC_SIM" > /tmp/sim_leak_check_obc.log 2>&1 &
OBC_PID=$!

sleep 0.5   # give OBC (comms bus master) a head start on bind()/listen(), same as the other integration tests

MallocStackLogging=1 "$ADCS_SIM" > /tmp/sim_leak_check_adcs.log 2>&1 &
ADCS_PID=$!

sleep "$RUN_SEC"   # let several real command/ACK/telemetry cycles happen

OBC_LEAKS=$(leaks "$OBC_PID" 2>&1) || true
ADCS_LEAKS=$(leaks "$ADCS_PID" 2>&1) || true

kill "$OBC_PID" "$ADCS_PID" 2>/dev/null || true
wait "$OBC_PID" "$ADCS_PID" 2>/dev/null || true

echo "=== obc_sim leaks ==="
echo "$OBC_LEAKS"
echo "=== adcs_sim leaks ==="
echo "$ADCS_LEAKS"

FAIL=0

if ! echo "$OBC_LEAKS" | grep -q "0 leaks for 0 total leaked bytes"; then
    echo "FAIL: obc_sim leaked memory -- see /tmp/sim_leak_check_obc.log and the 'obc_sim leaks' output above" >&2
    FAIL=1
fi

if ! echo "$ADCS_LEAKS" | grep -q "0 leaks for 0 total leaked bytes"; then
    echo "FAIL: adcs_sim leaked memory -- see /tmp/sim_leak_check_adcs.log and the 'adcs_sim leaks' output above" >&2
    FAIL=1
fi

exit $FAIL
