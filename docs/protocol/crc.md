# Data Integrity: CRC-16

## The Problem
Radio links and high-altitude environments are prone to bit-flips caused by interference or cosmic radiation. A single bit-flip in a command could be catastrophic.

## The Solution: CRC-16 CCITT
We append a 2-byte **Cyclic Redundancy Check (CRC)** to every packet. 

### How it Works (The Math)
- The entire packet is treated as one giant binary number.
- We perform "Binary Division" using a standardized **Polynomial (`0x1021`)**.
- The CRC is the **remainder** of this division.
- If the receiver calculates a different remainder than the one sent, the packet is discarded immediately.

### The "Sieve" Algorithm
Our implementation uses a bit-by-bit sliding window:
1.  Initialize a 16-bit register to `0xFFFF`.
2.  Shift the data through the register.
3.  If the top bit (MSB) is 1, "Subtract" (XOR) the polynomial.
4.  Repeat for every bit in the packet.

## Usage
The CRC is calculated over the **Header + Payload** but *excludes* the CRC field itself.
