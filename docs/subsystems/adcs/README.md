# ADCS Subsystem (Attitude Determination and Control System)

## Overview
The ADCS is the "Orientation Brain" of the satellite. It is responsible for determining the satellite's pose in space and commanding actuators (Reaction Wheels/Magnetorquers) to reach a target orientation.

## Task Architecture
The ADCS operates using two primary real-time tasks:

### 1. Control Task (`control_task.c`)
- **Frequency:** 50Hz (Deterministic).
- **Responsibility:** Reads sensors, executes PID control math, and drives actuators.
- **Pattern:** Uses `vTaskDelayUntil` to prevent timing drift.

### 2. Command Task (`command_task.c`)
- **Frequency:** Asynchronous / Event-driven.
- **Responsibility:** Monitors the command queue for instructions from the OBC (On-Board Computer).
- **Communication:** Translates raw packets into `ControlRequest` structures for the Control Task.

## State Machine
The ADCS cycles through states defined in `adcs_context.h`:
- `SAFE`: Low power, minimal control.
- `IDLE`: Powered on, waiting for targets.
- `TRANSITIONING`: Actively moving to a new orientation.
- `POINTING`: Maintaining a stable orientation (e.g., Sun-pointing).
