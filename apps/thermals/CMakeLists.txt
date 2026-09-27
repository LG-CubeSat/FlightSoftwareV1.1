add_executable(thermals_sim
    src/main.c
    src/freertos_hooks.c

    src/data/thermal_data.c
    src/data/thermal_sensor.c

    src/heater/prop_int_controller.c

    src/simulation/heater_sim.c
    src/simulation/thermal_model.c

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
    ${CMAKE_CURRENT_SOURCE_DIR}/include/data
    ${CMAKE_CURRENT_SOURCE_DIR}/include/heater
)

target_link_libraries(thermals_sim PRIVATE
    freertos_kernel
    pthread
    platform
    csp_interface
)