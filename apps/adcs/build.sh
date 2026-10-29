#!/usr/bin/env bash
# Build without flashing. Reuse the compiler on PATH, ARM_TOOLCHAIN_BIN, or CubeIDE.
set -euo pipefail
project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
repo_dir="$(cd -- "$project_dir/../.." && pwd)"
if ! command -v arm-none-eabi-gcc >/dev/null 2>&1 && [[ -z "${ARM_TOOLCHAIN_BIN:-}" ]]; then
    for compiler in /Applications/STM32CubeIDE.app/Contents/Eclipse/plugins/com.st.stm32cube.ide.mcu.externaltools.gnu-tools-for-stm32.*/tools/bin/arm-none-eabi-gcc; do
        if [[ -x "$compiler" ]]; then
            export ARM_TOOLCHAIN_BIN="${compiler%/*}"
            break
        fi
    done
fi
cmake -S "$project_dir" -B "$repo_dir/build-stm32" \
    -DCMAKE_TOOLCHAIN_FILE="$project_dir/cmake/arm-none-eabi.cmake" \
    -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build "$repo_dir/build-stm32" --target adcs_stm32 --parallel
