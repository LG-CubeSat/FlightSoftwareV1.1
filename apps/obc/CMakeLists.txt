add_executable(obc_sim
 src/main.c
)

target_link_libraries(obc_sim PRIVATE csp_interface)
