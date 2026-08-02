if (HW_MODE)
    add_library(platform STATIC
        real/spi/spi_driver.c
        )
    target_compile_definitions(platform PRIVATE HW_MODE)
else()
    add_library(platform STATIC
        sim/spi/v_bus.c
    )
    target_compile_definitions(platform PRIVATE SIM_MODE)
endif()

if (HW_MODE)
    target_include_directories(platform PUBLIC
        ${CMAKE_SOURCE_DIR}/platform/include
        ${CMAKE_CURRENT_SOURCE_DIR}/real/include
    )
else()
    target_include_directories(platform PUBLIC
        ${CMAKE_SOURCE_DIR}/platform/include
        ${CMAKE_CURRENT_SOURCE_DIR}/sim/include
    )
endif()