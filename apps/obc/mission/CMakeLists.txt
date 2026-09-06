add_executable(obc_mission
    src/main.c
    src/scheduler.c
    src/autonomy.c
    src/payload_commander.c
)

target_include_directories(obc_mission PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_link_libraries(obc_mission PRIVATE obc_ipc pthread platform)
