idf_component_register(
    SRCS "baremetal_utility.c"
    INCLUDE_DIRS "."
    REQUIRES php heap
    # baremetal_utility_module_entry is referenced only from a data table in another
    # component, which --gc-sections won't chase into an archive. Force the archive in.
    WHOLE_ARCHIVE
)

# Leans on PHP's macro-heavy headers; keep them under the same relaxed flags as
# the engine sources.
target_compile_options(${COMPONENT_LIB} PRIVATE -w -fpermissive)
