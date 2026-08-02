set(FREERTOS_KERNEL_DIR ${CMAKE_CURRENT_SOURCE_DIR}/kernels)
set(FREERTOS_PORT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/ports/posix)
set(FREERTOS_CONFIG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/config)

add_library(freertos_kernel STATIC
    ${FREERTOS_KERNEL_DIR}/src/tasks.c
    ${FREERTOS_KERNEL_DIR}/src/list.c
    ${FREERTOS_KERNEL_DIR}/src/queue.c
    ${FREERTOS_KERNEL_DIR}/src/timers.c
    ${FREERTOS_KERNEL_DIR}/src/event_groups.c
    ${FREERTOS_KERNEL_DIR}/src/croutine.c
    ${FREERTOS_KERNEL_DIR}/src/stream_buffer.c
    ${FREERTOS_KERNEL_DIR}/src/heap_4.c
    ${FREERTOS_PORT_DIR}/port.c
    ${FREERTOS_PORT_DIR}/utils/wait_for_event.c
)

target_include_directories(freertos_kernel PUBLIC
    ${FREERTOS_KERNEL_DIR}/include
    ${FREERTOS_PORT_DIR}
    ${FREERTOS_CONFIG_DIR}
)

target_link_libraries(freertos_kernel PUBLIC pthread)