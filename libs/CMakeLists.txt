add_library(sddv STATIC
    libs/sddv/sddv.c
    libs/sddv/rs8.c
)

target_include_directories(sddv PUBLIC libs/sddv)