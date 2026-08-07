add_executable(adcs_sim
    src/main.c
    src/freertos_hooks.c
    src/tasks/command_task.c
    src/tasks/control_task.c
    src/tasks/estimation_task.c
    src/tasks/housekeeping_task.c
    src/tasks/sensor_task.c
    src/tasks/telemetry_task.c
    src/communication/command_handler.c
)

target_include_directories(adcs_sim PRIVATE
    ${CMAKE_SOURCE_DIR}/rtos/kernels/include
    ${CMAKE_SOURCE_DIR}/rtos/ports/posix
    ${CMAKE_SOURCE_DIR}/rtos/config
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(adcs_sim PRIVATE freertos_kernel pthread platform csp_interface)
