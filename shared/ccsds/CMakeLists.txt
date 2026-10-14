# CCSDS space packet framing (LG profile) for the radio link.
# compression.c / stream.c from the full library are left out: compression
# needs libaec and the radio doesn't compress. compression_config.c carries
# the one config check profile.c needs.
add_library(ccsds STATIC
    src/compression_config.c
    src/crc32.c
    src/profile.c
    src/space_packet.c
    src/status.c
)

target_include_directories(ccsds
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_options(ccsds PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
