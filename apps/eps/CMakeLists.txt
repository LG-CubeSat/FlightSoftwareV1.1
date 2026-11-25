set(EPS_SIM_SOURCES
    src/main.c
    src/freertos_hooks.c
    src/data/eps_data.c

    src/tasks/fault_manager_task.c

    src/tasks/estimation_task.c
    src/tasks/housekeeping_task.c
    src/tasks/sensor_task.c
    src/tasks/telemetry_task.c
    src/communication/command_handler.c
)

add_executable(eps_sim ${EPS_SIM_SOURCES})

option(EPS_DEBUG_OUTPUT "Print EPS sensor/estimation snapshots" OFF)
target_compile_definitions(eps_sim PRIVATE
    EPS_DEBUG_OUTPUT=$<BOOL:${EPS_DEBUG_OUTPUT}>
)

target_include_directories(eps_sim PRIVATE
    ${CMAKE_SOURCE_DIR}/rtos/kernels/include
    ${CMAKE_SOURCE_DIR}/rtos/ports/posix
    ${CMAKE_SOURCE_DIR}/rtos/config
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/include/utils
    ${CMAKE_CURRENT_SOURCE_DIR}/include/data
)

target_link_libraries(eps_sim PRIVATE freertos_kernel pthread platform csp_interface)
