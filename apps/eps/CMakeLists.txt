set(EPS_SIM_SOURCES
    src/main.c
    src/freertos_hooks.c
    src/eps_data.c

    src/communication/telemetry.c

    src/manager/eps_manager.c
    src/manager/fault_manager.c

    src/tasks/estimation_task.c
    src/tasks/housekeeping_task.c
    src/tasks/sensor_task.c
    src/tasks/telemetry_task.c
)

add_executable(eps_sim ${EPS_SIM_SOURCES})

target_include_directories(eps_sim PRIVATE
    ${CMAKE_SOURCE_DIR}/rtos/kernels/include
    ${CMAKE_SOURCE_DIR}/rtos/ports/posix
    ${CMAKE_SOURCE_DIR}/rtos/config
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
)

target_link_libraries(eps_sim PRIVATE freertos_kernel pthread platform csp_interface)
