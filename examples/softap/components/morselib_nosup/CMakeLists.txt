# Copyright 2025 Morse Micro
# SPDX-License-Identifier: Apache-2.0

# This is an override of the standard morselib component that uses
# libmorse_nosupplicant.a instead of libmorse_nocrypto.a for SoftAP support

set(inc "include")

idf_component_register(INCLUDE_DIRS ${inc}
                       WHOLE_ARCHIVE)

if(${IDF_TARGET} STREQUAL "esp32s3")
    add_prebuilt_library(libmorse "${CMAKE_CURRENT_SOURCE_DIR}/../../../framework/morselib/lib/esp32s3/libmorse_nosupplicant.a"
                         PRIV_REQUIRES esp_rom mmpktmem)
elseif(${IDF_TARGET} STREQUAL "esp32c6")
    add_prebuilt_library(libmorse "${CMAKE_CURRENT_SOURCE_DIR}/../../../framework/morselib/lib/esp32c6/libmorse_nosupplicant.a"
                         PRIV_REQUIRES esp_rom mmpktmem)
elseif(${IDF_TARGET} STREQUAL "esp32c3")
    add_prebuilt_library(libmorse "${CMAKE_CURRENT_SOURCE_DIR}/../../../framework/morselib/lib/esp32c3/libmorse_nosupplicant.a"
                         PRIV_REQUIRES esp_rom mmpktmem)
else()
    message(FATAL_ERROR "ESP-IDF target ${IDF_TARGET} not supported, exiting.")
endif()

target_link_libraries(${COMPONENT_LIB} INTERFACE libmorse)
