# OBC (Raspberry Pi) I2C Bring-Up Guide

**Status:** written ahead of hardware arrival. Walk through this top to bottom once the Pi
Zero 2 W is physically in hand, before writing or running any of our own driver code against
it. Every step here uses stock Raspberry Pi OS tools — nothing from this repo — so a failure
at any step points at OS/wiring configuration, not our code.

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

## 6. Verify the bus electrically — before any of our code runs

With `i2c-tools` installed (step 3), scan the bus:

```bash
i2cdetect -y 1
```

- **With nothing wired up yet:** expect a grid of all `--` — an *empty* result is success here,
  it means the bus is alive and just has nothing attached.
- **An actual error** (e.g. `Error: Could not open file "/dev/i2c-1"`) means step 4 or 5 isn't
  done correctly — not a wiring problem. Fix that before assuming anything about hardware.
- **Once ADCS and/or Thermals are wired in** at their assigned I2C addresses (see
  `docs/satellite_architecture.md` — real I2C addresses are a separate concept from CSP
  addresses and still need assigning), their addresses should appear highlighted in the grid.
  This is the checkpoint that proves the physical bus and address wiring are correct,
  completely independent of any code in this repository.

---

## 7. Bus speed (note for later, not needed yet)

The Pi's I2C clock defaults to 100kHz. It can be raised (e.g. to 400kHz "fast mode") via
`dtparam=i2c_arm_baudrate=400000` in `config.txt`, if both MCU boards support running that
fast. Bus speed is still an open decision (see `docs/satellite_architecture.md`'s Open Items) —
don't change this until that's settled, and re-run step 6 after changing it to confirm the bus
still enumerates correctly at the new speed.

---

## 8. What's next

Once `i2cdetect` confirms the bus is alive (and, once boards are wired in, that they respond at
their assigned addresses), the next step is writing the actual master driver
(`platform/real/drivers/`'s OBC-side I2C implementation) against the Linux `i2c-dev` ioctl API.
See `docs/satellite_architecture.md` §4 item C.4 for where that fits in the overall bring-up
plan, and the discussion in-session about how real I2C's master-driven addressing model differs
from the current SIM transport's broadcast-and-filter design — that's the main design question
to resolve before writing the driver, not just an implementation detail.
