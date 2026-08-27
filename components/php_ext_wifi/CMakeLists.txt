# wifi -- scan/join a WiFi network (STA) and create one (SoftAP), from PHP.
#
# WiFi-capable SoCs: ESP32 / ESP32-S3 / C-series with a native radio, or an ESP32-P4 with a WiFi
# companion over ESP-HOSTED (see boards/esp32-p4/esp32-p4-wifi-c6). Opt-in, because the WiFi stack is
# heavy: compiled only when the project enables it (-DPHP_EXT_WIFI=ON, from [extensions.wifi]
# enabled = true). On a chip with no path to WiFi (e.g. esp32h2) the build fails early with a clear
# message (see the php component's extensions.cmake).
#
# The component always registers, but its REQUIRES list is gated on the PHP_EXT_WIFI cache var
# (NOT on IDF_TARGET) so ESP-IDF's early-expansion pass sees a stable requirements list -- and the
# heavy esp_wifi/esp_netif deps are only pulled in when the extension is actually built. The code in
# wifi.c compiles to an empty translation unit unless PHP_WIFI_BUILD is defined.

idf_component_register(
    SRCS "wifi.c"
    INCLUDE_DIRS "."
    # REQUIRES is unconditional so ESP-IDF's early-expansion pass sees a stable list (a conditional
    # here is evaluated with the wrong variables and drops esp_wifi). It's harmless when the extension
    # is off: nothing REQUIRES this component then, so it -- and esp_wifi with it -- is never built.
    REQUIRES php esp_wifi esp_netif esp_event nvs_flash
    # wifi_module_entry is pulled from a data table in the php component (out of --gc-sections' reach).
    WHOLE_ARCHIVE
)

if(PHP_EXT_WIFI AND NOT IDF_TARGET STREQUAL "esp32h2")
    # esp32p4 has no native radio but gets WiFi from a companion (e.g. ESP32-C6) over ESP-HOSTED:
    # esp_wifi_remote (pulled by the esp_hosted managed dependency) provides the esp_wifi_* symbols.
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_WIFI_BUILD=1)
    # wifi.c leans on PHP's macro-heavy headers; same relaxed flags as the engine.
    target_compile_options(${COMPONENT_LIB} PRIVATE -w -fpermissive)
endif()
