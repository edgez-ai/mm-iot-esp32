# Copyright Morse Micro 2023
#
# SPDX-License-Identifier: Apache-2.0
#
set(inc
    "lwip"
    ".")
set(src
    "lwip/mmipal_lwip.c"
    "lwip/mmnetif.c")

idf_component_register(INCLUDE_DIRS ${inc}
                       SRCS ${src}
                       PRIV_REQUIRES driver morselib mm_shims lwip)

# Enable lwIP netif callbacks required for mmipal
target_compile_definitions(${COMPONENT_LIB} PRIVATE
    LWIP_NETIF_LINK_CALLBACK=1
    LWIP_NETIF_STATUS_CALLBACK=1)
