add_executable(adcs_sim src/main.c)

target_include_directories(adcs_sim PRIVATE
    ${CMAKE_SOURCE_DIR}/rtos/kernels/include
    ${CMAKE_SOURCE_DIR}/rtos/ports/posix
    ${CMAKE_SOURCE_DIR}/rtos/config
)

target_link_libraries(adcs_sim PRIVATE freertos_kernel pthread)