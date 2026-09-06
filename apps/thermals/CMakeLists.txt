add_executable(thermals_sim
    src/main.c
    src/freertos_hooks.c
    src/thermal_data.c
    src/thermal_sensor.c
    src/tasks/sensor_read_task.c
    src/tasks/thermal_write_task.c
)

target_include_directories(thermals_sim PRIVATE
    ${CMAKE_SOURCE_DIR}/rtos/kernels/include
    ${CMAKE_SOURCE_DIR}/rtos/ports/posix
    ${CMAKE_SOURCE_DIR}/rtos/config
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(thermals_sim PRIVATE freertos_kernel pthread platform)
