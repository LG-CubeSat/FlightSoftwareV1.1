add_executable(obc_radio src/main.c)
target_include_directories(obc_radio PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(obc_radio PRIVATE obc_ipc platform pthread obc_logging)
