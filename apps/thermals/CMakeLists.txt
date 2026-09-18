add_executable(thermals_sim
    src/main.c
    src/freertos_hooks.c
    src/thermal_data.c
    src/thermal_sensor.c
    src/communication/command_handler.c
    src/tasks/command_task.c
    src/tasks/housekeeping_task.c
    src/tasks/sensor_read_task.c
    src/tasks/telemetry_task.c
    src/tasks/heater_set_task.c
)

target_include_directories(thermals_sim PRIVATE
    ${CMAKE_SOURCE_DIR}/rtos/kernels/include
    ${CMAKE_SOURCE_DIR}/rtos/ports/posix
    ${CMAKE_SOURCE_DIR}/rtos/config
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(thermals_sim PRIVATE freertos_kernel pthread platform csp_interface)
