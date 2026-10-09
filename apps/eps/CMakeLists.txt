add_executable(eps_sim
    src/main.c
    src/freertos_hooks.c
    src/tasks/estimation_task.c
    src/tasks/housekeeping_task.c
    src/tasks/sensor_task.c
    src/tasks/telemetry_task.c
)

target_include_directories(eps_sim PRIVATE
    ${CMAKE_SOURCE_DIR}/rtos/kernels/include
    ${CMAKE_SOURCE_DIR}/rtos/ports/posix
    ${CMAKE_SOURCE_DIR}/rtos/config
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(eps_sim PRIVATE freertos_kernel pthread platform csp_interface)
