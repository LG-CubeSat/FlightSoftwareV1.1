#!/bin/sh
# macOS stand-in for LeakSanitizer: LSan itself is not implemented on this
# platform (confirmed via `clang -fsanitize=leak` -> "unsupported option ...
# for target 'arm64-apple-darwin'", and ASan's detect_leaks=1 likewise
# reports "not supported on this platform"). `leaks --atExit` is the native
# equivalent -- it runs the given command and inspects its heap right as it
# exits.
#
# MallocStackLogging=1 is required in the child's environment or `leaks`
# silently reports "0 leaks" even when there are real ones -- verified by
# hand: a deliberately leaked malloc() was missed without it, found with it,
# stack trace and all.
#
# Exit code passes through from `leaks` itself: 0 if clean, non-zero if it
# found anything, so CTest sees this like any other pass/fail test command.
export MallocStackLogging=1
exec leaks --atExit -- "$@"
