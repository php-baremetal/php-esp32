# s3_onboard_rgb -- ESP32-S3-only extension for the board's onboard WS2812 RGB LED.
#
# The component always registers (a stable REQUIRES list keeps ESP-IDF's early-expansion pass
# happy), but s3_onboard_rgb.c compiles to an empty translation unit unless S3_ONBOARD_RGB_BUILD
# is defined -- which happens only when the project enables it (-DPHP_EXT_S3_ONBOARD_RGB=ON, from
# [extensions.s3_onboard_rgb] enabled = true) AND the target is esp32s3. Enabling the flag on a
# non-S3 target fails the build early with a clear message (see the php component's extensions.cmake),
# so the module registration in internal_functions.c and the code here are always in step.
#
# The data pin defaults to GPIO 48 (the pin on most S3 dev boards); override per project with
# [extensions.s3_onboard_rgb] pin = N, which flash-tool passes as -DPHP_S3_RGB_GPIO.

idf_component_register(
    SRCS "s3_onboard_rgb.c"
    INCLUDE_DIRS "."
    # esp_driver_rmt drives the WS2812 straight from the RMT peripheral (no external led_strip
    # component). The module_entry symbol is pulled from a data table in the php component, which
    # --gc-sections won't chase into an archive -> WHOLE_ARCHIVE.
    REQUIRES php esp_driver_rmt
    WHOLE_ARCHIVE
)

if(PHP_EXT_S3_ONBOARD_RGB AND IDF_TARGET STREQUAL "esp32s3")
    if(NOT DEFINED PHP_S3_RGB_GPIO OR "${PHP_S3_RGB_GPIO}" STREQUAL "")
        set(PHP_S3_RGB_GPIO 48)
    endif()
    target_compile_definitions(${COMPONENT_LIB} PRIVATE
        S3_ONBOARD_RGB_BUILD=1
        S3_ONBOARD_RGB_GPIO=${PHP_S3_RGB_GPIO})
    # s3_onboard_rgb.c leans on PHP's macro-heavy headers; same relaxed flags as the engine.
    target_compile_options(${COMPONENT_LIB} PRIVATE -w -fpermissive)
endif()
