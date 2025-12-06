# Copyright 2023 Morse Micro
# SPDX-License-Identifier: Apache-2.0
set(src
    "src/mm_app_loadconfig.c"
    "src/mm_app_common.c"
    "src/mm_app_regdb.c"
    "src/softap.c")

set(inc
    "src")

idf_component_register(SRCS ${src}
                       INCLUDE_DIRS ${inc}
                       PRIV_REQUIRES driver morselib_nosup mm_shims mmipal lwip hostap_mm)
