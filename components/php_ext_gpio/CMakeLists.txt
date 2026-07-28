idf_component_register(
    SRCS "gpio.c"
    INCLUDE_DIRS "."
    REQUIRES php esp_driver_gpio
    # gpio_module_entry is referenced only from a data table in another component,
    # which --gc-sections won't chase into an archive. Force the whole archive in.
    WHOLE_ARCHIVE
)

# gpio.c leans on PHP's macro-heavy headers; keep them under the same relaxed
# flags as the engine sources.
target_compile_options(${COMPONENT_LIB} PRIVATE -w -fpermissive)
