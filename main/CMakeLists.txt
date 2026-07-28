idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES php php_ext_gpio fatfs esp_driver_sdmmc sdmmc vfs
)
