# Event bus. The C core (src/) is portable C -- the test/ sources build on the host with gcc, so they
# are not part of the component build. php/ is the PHP facade (Baremetal\Event + Events), target-only;
# events_module_entry is pulled from a data table in the php component, out of --gc-sections' reach,
# so the whole archive is forced in.
idf_component_register(
    SRCS "src/pool.c" "src/dispatch.c" "src/isr_pool.c" "php/php_events.c"
    INCLUDE_DIRS "include"
    REQUIRES php esp_timer esp_driver_gpio
    WHOLE_ARCHIVE
)

# php_events.c leans on PHP's macro-heavy headers; keep it under the same relaxed flags as the engine.
set_source_files_properties("php/php_events.c" PROPERTIES COMPILE_OPTIONS "-w;-fpermissive")
