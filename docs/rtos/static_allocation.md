# Static Memory Allocation in FreeRTOS

## The Policy
**Dynamic memory allocation (`malloc`, `free`, `new`) is strictly forbidden in the Flight Software.**

## Why Static?
1.  **Determinism:** We know exactly how much memory the satellite uses the moment it boots.
2.  **No Fragmentation:** There is no risk of a "Heap Overflow" or "Memory Leak" during a long-duration mission.
3.  **Compile-Time Verification:** If we run out of RAM, the compiler tells us *before* we launch.

## Implementation Pattern
Instead of `xTaskCreate`, we use `xTaskCreateStatic`. This requires the developer to provide two buffers:
1.  **TCB (Task Control Block):** A `StaticTask_t` buffer for kernel metadata.
2.  **Stack:** A `StackType_t` array for local variables and function calls.

### Example Creator:
```c
static StaticTask_t xTaskBuffer;
static StackType_t xStack[STACK_SIZE];

TaskHandle_t create_my_task_static(void) {
    return xTaskCreateStatic(func, "NAME", STACK_SIZE, NULL, PRIO, xStack, &xTaskBuffer);
}
```

## Mandatory Callbacks
When `configSUPPORT_STATIC_ALLOCATION` is enabled, the developer **must** provide memory for the system's internal tasks:
- `vApplicationGetIdleTaskMemory()`
- `vApplicationGetTimerTaskMemory()`
