set(src
    "src/mm_app_regdb.c"
    "src/sta_connect.c")

idf_component_register(SRCS ${src} PRIV_REQUIRES driver morselib mm_shims)

target_compile_definitions(${COMPONENT_LIB} PRIVATE COUNTRY_CODE="US")
