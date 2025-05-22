file(GLOB_RECURSE COMPONENT_SOURCES "*.c")

idf_component_register(
    SRCS ${COMPONENT_SOURCES}
    INCLUDE_DIRS "."
)

target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-unused-const-variable)
