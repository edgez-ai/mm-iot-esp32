# Copyright 2025 Morse Micro
# SPDX-License-Identifier: Apache-2.0

# Collect all hostap source files
file(GLOB_RECURSE HOSTAP_SRCS
    "src/ap/*.c"
    "src/common/*.c"
    "src/utils/*.c"
    "src/crypto/*.c"
    "src/eap_common/*.c"
    "src/eap_server/*.c"
    "src/eapol_auth/*.c"
    "src/radius/*.c"
)

# Add Morse-specific hostap implementation files
list(APPEND HOSTAP_SRCS
    "crypto_mbedtls_mm.c"
    "crypto_mbedtls_dpp.c"
    "drivers_morse.c"
    "morse_stubs.c"
    "os_mmosal.c"
)

# Exclude files that are not needed or cause build issues
list(FILTER HOSTAP_SRCS EXCLUDE REGEX ".*_module_tests\\.c$")
list(FILTER HOSTAP_SRCS EXCLUDE REGEX ".*/hostapd_cli\\.c$")
list(FILTER HOSTAP_SRCS EXCLUDE REGEX ".*/main\\.c$")
list(FILTER HOSTAP_SRCS EXCLUDE REGEX ".*/nt_password_hash\\.c$")
list(FILTER HOSTAP_SRCS EXCLUDE REGEX ".*/hlr_auc_gw\\.c$")
list(FILTER HOSTAP_SRCS EXCLUDE REGEX ".*/sae_pk_gen\\.c$")
list(FILTER HOSTAP_SRCS EXCLUDE REGEX ".*/preauth_auth\\.c$")
list(FILTER HOSTAP_SRCS EXCLUDE REGEX ".*/eap_register\\.c$")
list(FILTER HOSTAP_SRCS EXCLUDE REGEX ".*_none\\.c$")
list(FILTER HOSTAP_SRCS EXCLUDE REGEX ".*_unix\\.c$")
list(FILTER HOSTAP_SRCS EXCLUDE REGEX ".*_win32\\.c$")

set(HOSTAP_INCLUDES
    "."
    "src"
    "src/ap"
    "src/common"
    "src/utils"
    "src/crypto"
    "src/eap_common"
    "src/eap_server"
    "src/eapol_auth"
    "src/radius"
)

# Define compile options for hostap
set(HOSTAP_COMPILE_OPTIONS
    -DCONFIG_NO_STDOUT_DEBUG
    -DCONFIG_IEEE80211AX
    -DCONFIG_IEEE80211W
    -DCONFIG_SHA256
    -DCONFIG_INTERNAL_LIBTOMMATH
    -DCONFIG_CRYPTO_INTERNAL
    -DCONFIG_TLS=none
    -DCONFIG_NO_RADIUS
    -DCONFIG_NO_ACCOUNTING
    -DCONFIG_NO_VLAN
    -DCONFIG_NO_WPA_PASSPHRASE
    -DCONFIG_S1G
    -DENABLE_HOSTAP_CRYPTO_API_MANGLING=1
    -Wno-unused-parameter
    -Wno-sign-compare
    -Wno-old-style-declaration
    -Wno-type-limits
    -Wno-pointer-sign
)

idf_component_register(
    SRCS ${HOSTAP_SRCS}
    INCLUDE_DIRS ${HOSTAP_INCLUDES}
    PRIV_REQUIRES mbedtls lwip
    REQUIRES morselib mm_shims mmipal
)

# Apply compile options
target_compile_options(${COMPONENT_LIB} PRIVATE ${HOSTAP_COMPILE_OPTIONS})

# Include the symbol mangling header to avoid conflicts with ESP-IDF's wpa_supplicant
target_compile_options(${COMPONENT_LIB} PRIVATE -include ${CMAKE_CURRENT_SOURCE_DIR}/esp_symbol_mangle.h)
