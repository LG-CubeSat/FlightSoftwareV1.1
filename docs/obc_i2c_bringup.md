# OBC (Raspberry Pi) I2C Bring-Up Guide

**Status:** updated for the Raspberry Pi Zero 2 W and the STM32G431 ADCS bring-up firmware.
The ADCS board uses seven-bit I2C address `0x2A`; STM32 HAL displays the same address shifted
left as `0x54` in `OAR1`.

---

## 1. What you'll need

- Raspberry Pi Zero 2 W
- microSD card (8GB+) and a way to write it from your dev machine
- Raspberry Pi Imager (on your dev machine) — https://www.raspberrypi.com/software/
- A way to reach the Pi headless: Wi-Fi + SSH (set up during flashing, see below) is the
  normal path for a Zero 2 W. Keep a USB-to-TTL serial console cable in mind as a fallback if
  network config ever goes wrong and you need a console with no network at all.

---

## 2. Flash the OS

1. Open Raspberry Pi Imager. Choose **Raspberry Pi OS Lite (64-bit)** — no desktop environment
   needed, this board runs headless.
2. Before writing, open the advanced options (gear icon, or Ctrl+Shift+X): set a hostname,
   enable SSH (password or your public key), and enter Wi-Fi credentials if you're not using
   a wired connection. Set locale/timezone while you're there.
3. Write the image, boot the Pi, and confirm you can reach it: `ssh <hostname>.local` (or by
   IP address if `.local` mDNS resolution doesn't work on your network).

---

## 3. Base OS update + build toolchain

```bash
sudo apt update && sudo apt full-upgrade -y
sudo apt install -y cmake build-essential git i2c-tools
```

Then clone this repo per `README.md`'s "Clone" section (it uses git submodules for libcsp,
ssdv, and CCSDS_121.0 — `git clone --recurse-submodules ...`). Confirm the SIM build still
works here first (`cmake -S . -B build -DHW_MODE=OFF && cmake --build build`) before touching
I2C at all — that isolates "the Pi can build this project" from "the I2C bus works," so a
problem later is easier to place.

---

## 4. Enable the I2C interface

The interface is present on the SoC but disabled by default.

- Easiest: `sudo raspi-config` → **Interface Options** → **I2C** → enable → reboot.
- Equivalent manual edit: add `dtparam=i2c_arm=on` to `/boot/firmware/config.txt`, then reboot.

Confirm the device node exists:

```bash
ls /dev/i2c*
```

You should see `/dev/i2c-1` — the Pi's normal user-facing I2C bus. (`/dev/i2c-0` also exists on
most Pi models but is reserved for HAT EEPROM auto-detection; don't use it for this.)

---

## 5. Permissions

`/dev/i2c-1` is owned by `root:i2c` by default — a non-root program (like ours will be) can't
open it until your user is in that group:

```bash
sudo usermod -aG i2c $USER
```

Log out and back in (or reboot) for the group membership to take effect. Skipping this step
shows up later as every `open("/dev/i2c-1", ...)` failing with `EACCES`, which can look like a
driver bug if you don't already know to check group membership first.

---

## 6. Wire and verify the bus electrically

Power both boards normally and connect a common ground. The I2C signals are 3.3 V logic; do
not connect a Pi 5 V pin to SDA or SCL.

| Signal | Raspberry Pi Zero 2 W | ADCS J8 |
|---|---|---|
| SDA | physical pin 3, GPIO2/SDA1 | pin 3, STM32 PB7/I2C1_SDA |
| SCL | physical pin 5, GPIO3/SCL1 | pin 4, STM32 PA15/I2C1_SCL |
| GND | any ground pin, for example physical pin 6 | board ground |

With both boards powered and idle, SDA and SCL should each measure close to 3.3 V. A line
stuck near 0 V means the bus is shorted, miswired, or one device is holding it low. The ADCS
PCB includes 4.7 kOhm pull-ups, so do not add stronger pull-ups during initial bring-up.

First confirm the Pi has selected the correct pins and bus:

```bash
pinctrl get 2,3
ls -l /dev/i2c-1
```

GPIO2 and GPIO3 should show their I2C alternate function and both should read high while idle.

With `i2c-tools` installed (step 3), scan the bus:

```bash
sudo i2cdetect -y 1 0x2a 0x2a
```

- **With the flashed and running ADCS board connected:** expect `2a` in row `20`, column `a`.
- **With nothing wired up yet:** expect `--` at `0x2A`; that only proves no slave acknowledged.
- **An actual error** (e.g. `Error: Could not open file "/dev/i2c-1"`) means step 4 or 5 isn't
  done correctly — not a wiring problem. Fix that before assuming anything about hardware.

`i2cdetect` uses probe transactions that are not the actual ADCS protocol, so the definitive
test is the repeated-start `GET_STATUS` transaction:

```bash
sudo i2ctransfer -y 1 w1@0x2a 0x01 r5
```

With no response frame queued, the exact result is:

```text
0xa5 0x01 0x00 0x00 0x00
```

That transaction is one Linux `I2C_RDWR` operation: write command `0x01`, issue a repeated
START without releasing the bus, and read the five-byte reply. Separate shell commands do not
test repeated-start behavior.

---

## 7. Bus speed (note for later, not needed yet)

The Pi's I2C clock defaults to 100kHz. It can be raised (e.g. to 400kHz "fast mode") via
`dtparam=i2c_arm_baudrate=400000` in `config.txt`, if both MCU boards support running that
fast. Bus speed is still an open decision (see `docs/satellite_architecture.md`'s Open Items) —
don't change this until that's settled, and re-run step 6 after changing it to confirm the bus
still enumerates correctly at the new speed.

---

## 8. Build and run the repository test

From the repository root on the Pi:

```bash
cmake -S . -B build-hw -DHW_MODE=ON -DBUILD_TESTS=OFF
cmake --build build-hw --target obc_adcs_i2c_test -j4
./build-hw/bin/obc_adcs_i2c_test status
./build-hw/bin/obc_adcs_i2c_test roundtrip
```

The first program command validates the five-byte status response. `roundtrip` sends a framed
payload with `WRITE_FRAME`, polls `GET_STATUS`, reads it back with `READ_FRAME`, and verifies
the returned source, destination, length, and payload. During the test, the ADCS UART should
show address, receive, transmit, completion, malformed-transaction, and error counters.

If `/dev/i2c-1` is still owned by `root:i2c` and the current login has not picked up its new
group membership, temporarily prefix the two program commands with `sudo`; logging out and
back in is the lasting fix.
