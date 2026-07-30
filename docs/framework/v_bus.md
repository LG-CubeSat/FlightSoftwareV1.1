# Virtual Bus (v_bus) Architecture

## Overview
The **v_bus** is our project's **Hardware Abstraction Layer (HAL)** for inter-processor communication. In a flight configuration, the On-Board Computer (OBC) and the ADCS communicate via a physical SPI bus. In our current **Software-in-the-Loop (SIL)** simulator, the `v_bus` replaces that copper wire with a high-fidelity Unix Domain Socket.

## The Design Philosophy: "Everything is a File"
In Unix-based systems (macOS/Linux), communication between two programs can be treated like writing to a file. We use **Unix Domain Sockets (UDS)** because they provide:
1.  **Deterministic Ordering:** Data sent as "A, B, C" is guaranteed to arrive as "A, B, C".
2.  **Zero Packet Loss:** Unlike UDP, the OS ensures no bytes are dropped in the "Virtual Wire".
3.  **Synchronization:** The Master and Slave must both be "plugged in" for the system to start.

---

## Master vs. Slave Roles
We mimic the physical SPI relationship:

### 1. The Master (OBC)
- **Role:** The "Server" and Bus Controller.
- **Initialization:** Calls `v_bus_init(1)`.
- **Logic:** 
  - Deletes any old "wires" (`unlink`).
  - Creates the socket file at `/tmp/v_bus.sock`.
  - **Blocks** at `accept()` until the ADCS connects.

### 2. The Slave (ADCS)
- **Role:** The "Client" and Peripheral.
- **Initialization:** Calls `v_bus_init(0)`.
- **Logic:** 
  - Attempts to `connect()` to the OBC.
  - If the OBC isn't ready, it patiently waits and retries every 100ms.

---

## API Reference

### `VBusStatus_t v_bus_init(int is_master)`
Initializes the communication channel.
- **Master:** Sets up the listener and waits for a connection.
- **Slave:** Connects to the existing listener.

### `int v_bus_send(const uint8_t *data, uint16_t length)`
Sends raw bytes across the bus.
- **Returns:** Number of bytes sent, or `-1` if the "wire" is broken (e.g., the other process crashed).
- **Equivalent:** `HAL_SPI_Transmit`

### `int v_bus_receive(uint8_t *buffer, uint16_t max_length)`
Blocks the calling task until data arrives.
- **Returns:** Number of bytes received, or `-1` on failure.
- **Equivalent:** `HAL_SPI_Receive`

---

## Aerospace Reliability: The "Pull-Up" Pattern
Just as a physical wire uses a pull-up resistor to signal a "hang high" state on failure, our `v_bus` functions return `-1`. 
- If `v_bus_send` returns `-1`, the flight software interprets this as a **Bus Fault**. 
- The system should then trigger a "Re-initialization" or enter **Safe Mode**.

---

## Hardware Migration Path
To move from the Mac Mini simulator to the **STM32 BluePill**, we only modify the *implementation* of `v_bus.c`:

1.  **Delete** the `<sys/socket.h>` code.
2.  **Include** the `<stm32f1xx_hal.h>`.
3.  **Re-implement** `v_bus_send` using `HAL_SPI_Transmit`.
4.  **No changes** are required in the ADCS tasks or OBC logic.
