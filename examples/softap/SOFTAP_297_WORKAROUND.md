# SoftAP with 2.9.7 - Workaround Implementation

This directory contains the necessary workaround to make SoftAP work with version 2.9.7 of the MM-IoT-ESP32 SDK.

## Overview

The workaround involves:
1. **Pulling hostap code from MM-IoT-SDK** - The hostap implementation needed for AP mode
2. **Linking with libmorse_nosupplicant.a** - Using the no-supplicant library variant
3. **Symbol mangling** - Renaming symbols that collide with ESP-IDF's built-in wpa_supplicant

## Components Added

### 1. `components/hostap_mm/`
Contains the hostap code from MM-IoT-SDK with:
- AP management functions (`src/ap/`)
- Common utilities (`src/common/`)
- Cryptography (`src/crypto/`)
- EAPOL authentication (`src/eapol_auth/`)
- RADIUS client (`src/radius/`)
- Morse-specific implementations (drivers_morse.c, os_mmosal.c, etc.)
- **Symbol mangling header** (`esp_symbol_mangle.h`) - Prefixes hostap symbols with 'mm_' to avoid conflicts

### 2. `components/morselib_nosup/`
An override of the standard morselib component that links with `libmorse_nosupplicant.a` instead of `libmorse_nocrypto.a`.

## How It Works

### Symbol Mangling
The `esp_symbol_mangle.h` header is automatically included during compilation via the `-include` compiler flag. This header renames hostap symbols that would otherwise collide with ESP-IDF's wpa_supplicant component.

For example:
```c
#define hostapd_init mm_hostapd_init
#define wpa_receive mm_wpa_receive
#define ieee802_11_mgmt mm_ieee802_11_mgmt
```

This ensures that Morse Micro's hostap implementation coexists with ESP-IDF's built-in WiFi stack without linker conflicts.

### Library Selection
The `morselib_nosup` component ensures the project links with the appropriate variant of the Morse library that excludes the supplicant code, which is necessary for AP mode operation.

## Building

Build as normal:
```bash
idf.py build
idf.py flash monitor
```

## Files Modified

1. **main/CMakeLists.txt** - Updated to require `morselib_nosup` and `hostap_mm` components
2. **Added components/hostap_mm/** - Hostap implementation with symbol mangling
3. **Added components/morselib_nosup/** - Library variant selector

## Note

This is a temporary workaround for version 2.9.7. Future releases will have native AP mode support that won't require these manual steps.
