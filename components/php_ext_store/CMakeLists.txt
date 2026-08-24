idf_component_register(
    SRCS "store.c"
    INCLUDE_DIRS "."
    REQUIRES php nvs_flash
    # store_module_entry is referenced only from a data table in another component,
    # which --gc-sections won't chase into an archive. Force the whole archive in.
    WHOLE_ARCHIVE
)

# store.c leans on PHP's macro-heavy headers; keep them under the same relaxed
# flags as the engine sources (same as php_ext_gpio).
target_compile_options(${COMPONENT_LIB} PRIVATE -w -fpermissive)
