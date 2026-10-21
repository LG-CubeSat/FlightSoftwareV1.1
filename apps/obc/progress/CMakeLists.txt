add_library(obc_progress STATIC
    src/obc_progress.c
)

target_include_directories(obc_progress
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(obc_progress
    PUBLIC
        pthread
)