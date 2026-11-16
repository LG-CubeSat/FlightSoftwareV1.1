#!/usr/bin/env bash
# Read-only snapshot of the Pi GPIOs used by the onboard E22.
set -u

printf 'E22 GPIO snapshot: %s\n' "$(date -Is)"
printf 'Host: %s\n' "$(hostname)"
printf 'GPIO numbering: BCM (not physical header pins)\n'
printf 'Expected assignment: GPIO14=soft RX, GPIO15=soft TX, GPIO23=M1, GPIO24=M0, GPIO25=AUX\n'
printf '\nGPIO function, level, and pull:\n'

if command -v pinctrl >/dev/null 2>&1; then
    for pin in 14 15 23 24 25; do pinctrl get "$pin"; done
elif command -v raspi-gpio >/dev/null 2>&1; then
    for pin in 14 15 23 24 25; do raspi-gpio get "$pin"; done
else
    printf 'Neither pinctrl nor raspi-gpio is installed.\n'
fi

if command -v gpioinfo >/dev/null 2>&1; then
    printf '\nGPIO character-device line ownership:\n'
    gpioinfo gpiochip0 2>&1
fi

if command -v pigs >/dev/null 2>&1; then
    printf '\npigpio digital levels (0=low, 1=high):\n'
    for pin in 14 15 23 24 25; do
        printf 'GPIO%s: ' "$pin"
        pigs r "$pin" 2>&1
    done
fi

printf '\nProcess state:\n'
systemctl is-active obc.service 2>&1 || true
pgrep -af 'obc_supervisor|radio_hardware_test|pigpiod' || true

if command -v git >/dev/null 2>&1 && git rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    printf '\nFlight software revision: '
    git rev-parse --short HEAD
    if [ -d libs/RadioHandler ]; then
        printf 'RadioHandler revision: '
        git -C libs/RadioHandler rev-parse --short HEAD
        printf 'RadioHandler working tree:\n'
        git -C libs/RadioHandler status --short
    fi
fi
