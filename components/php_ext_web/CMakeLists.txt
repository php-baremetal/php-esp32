# web -- Baremetal\Http\Request/\Response + serve_http() (the `http` source for event-driven).
#
# Like the other hardware extensions, the component always registers (a stable REQUIRES list keeps
# ESP-IDF's early-expansion pass happy) but compiles to an empty translation unit unless PHP_WEB_BUILD
# is defined -- which happens only when the project enables it (-DPHP_EXT_WEB=ON, from [extensions.web]
# enabled = true). The module registration in internal_functions.c is gated on the same flag.

idf_component_register(
    SRCS "src/php_web.c"
    INCLUDE_DIRS "include"
    # php for the engine headers; event_bus for the reactor queue + the request/response dispatch;
    # esp_http_server for the HTTP transport. The module_entry symbol is pulled from a data table in
    # the php component, out of --gc-sections' reach -> WHOLE_ARCHIVE.
    REQUIRES php event_bus esp_http_server
    WHOLE_ARCHIVE
)

if(PHP_EXT_WEB)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE PHP_WEB_BUILD=1)
    target_compile_options(${COMPONENT_LIB} PRIVATE -w -fpermissive)
endif()
