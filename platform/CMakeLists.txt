if (HW_MODE)
    add_library(platform STATIC
        real/drivers/comms_i2c.c
        real/drivers/board_reset.c
        real/drivers/board_shutdown.c
        real/drivers/camera.c
        )
    target_compile_definitions(platform PRIVATE HW_MODE)
else()
    add_library(platform STATIC
        sim/drivers/comms_i2c.c
        sim/drivers/board_reset.c
        sim/drivers/board_shutdown.c
        sim/drivers/camera.c
        sim/drivers/radio.c
    )
    target_compile_definitions(platform PRIVATE SIM_MODE)
endif()

if (HW_MODE)
    target_include_directories(platform PUBLIC
        ${CMAKE_SOURCE_DIR}/shared/interfaces
        ${CMAKE_CURRENT_SOURCE_DIR}/real/include
    )
else()
    target_include_directories(platform PUBLIC
        ${CMAKE_SOURCE_DIR}/shared/interfaces
        ${CMAKE_CURRENT_SOURCE_DIR}/sim/include
    )
endif()

# frame_serialize/frame_deserialize are medium-agnostic (shared/interfaces/frame.c)
# -- both the sim and real backends encode/decode the same wire format.
target_link_libraries(platform PUBLIC frame_codec)