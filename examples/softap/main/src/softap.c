/*
 * Copyright 2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Soft AP example for the ESP32 MM-IoT port.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "mmosal.h"
#include "mmhal.h"
#include "mmwlan.h"
#include "mmipal.h"

#include "mm_app_common.h"
#include "mm_app_loadconfig.h"

#ifndef MM_UNUSED
#define MM_UNUSED(x) (void)(x)
#endif

/* ------------------------ Default configuration ------------------------ */

/* Default country code for regulatory domain. */
#ifndef COUNTRY_CODE
#define COUNTRY_CODE US
#endif

/* Default IPv4 configuration for the Soft AP interface. */
#ifndef STATIC_LOCAL_IP
#define STATIC_LOCAL_IP                 "192.168.1.1"
#endif
#ifndef STATIC_GATEWAY
#define STATIC_GATEWAY                  "192.168.1.1"
#endif
#ifndef STATIC_NETMASK
#define STATIC_NETMASK                  "255.255.255.0"
#endif

/* Default SSID/Security configuration. */
#ifndef SOFTAP_SSID
#define SOFTAP_SSID SoftAP
#endif
#ifndef SAE_PASSPHRASE
#define SAE_PASSPHRASE 12345678
#endif
#ifndef SECURITY_TYPE
#define SECURITY_TYPE                   MMWLAN_OPEN
#endif
#ifndef PMF_MODE
#define PMF_MODE                        MMWLAN_PMF_DISABLED
#endif

/* Default channel configuration. */
#ifndef OP_CLASS
#define OP_CLASS                        (71) /* US: channel 44 uses op_class 71 (8 MHz) */
#endif
#ifndef S1G_CHANNEL
#define S1G_CHANNEL                     (44) /* US: channel 44 @ 924 MHz */
#endif
#ifndef PRIMARY_BW_MHZ
#define PRIMARY_BW_MHZ                  (0)  /* 0 = auto */
#endif
#ifndef PRIMARY_1MHZ_CHANNEL_INDEX
#define PRIMARY_1MHZ_CHANNEL_INDEX      (0)
#endif

/** Stringify macro. Do not use directly; use @ref STRINGIFY(). */
#define _STRINGIFY(x) #x
/** Convert the content of the given macro to a string. */
#define STRINGIFY(x) _STRINGIFY(x)

/** Handler for Soft AP STA status callback. */
static void handle_softap_sta_status(const struct mmwlan_softap_sta_status *sta_status, void *arg)
{
    MM_UNUSED(sta_status);
    MM_UNUSED(arg);

    printf("STA status updated\n");
}

/**
 * Load IPv4 settings for the Soft AP interface.
 */
static void load_softap_mmipal_init_args(struct mmipal_init_args *args)
{
    (void)mmosal_safer_strcpy(args->ip_addr, STATIC_LOCAL_IP, sizeof(args->ip_addr));
    (void)mmosal_safer_strcpy(args->netmask, STATIC_NETMASK, sizeof(args->netmask));
    (void)mmosal_safer_strcpy(args->gateway_addr, STATIC_GATEWAY, sizeof(args->gateway_addr));
}

/**
 * Load Soft AP WLAN settings.
 */
static void load_mmwlan_softap_args(struct mmwlan_softap_args *softap_args)
{
    (void)mmosal_safer_strcpy((char *)softap_args->ssid, STRINGIFY(SOFTAP_SSID),
                              sizeof(softap_args->ssid));
    softap_args->ssid_len = strlen((char *)softap_args->ssid);

    (void)mmosal_safer_strcpy(softap_args->passphrase, STRINGIFY(SAE_PASSPHRASE),
                              sizeof(softap_args->passphrase));
    softap_args->passphrase_len = strlen(softap_args->passphrase);

    softap_args->security_type = SECURITY_TYPE;
    softap_args->pmf_mode = PMF_MODE;
    softap_args->op_class = OP_CLASS;
    softap_args->s1g_chan_num = S1G_CHANNEL;
    softap_args->pri_bw_mhz = PRIMARY_BW_MHZ;
    softap_args->pri_1mhz_chan_idx = PRIMARY_1MHZ_CHANNEL_INDEX;
    
    /* Explicitly set beacon interval and DTIM period (0 means use defaults) */
    softap_args->beacon_interval_tus = 100;  /* 100 TUs = 102.4 ms */
    softap_args->dtim_period = 1;            /* DTIM every beacon */
}

static void load_mmwlan_settings_softap(void)
{
    /* No additional settings to load - using defaults */
}

static const char *status_to_str(enum mmwlan_status status)
{
    switch (status)
    {
        case MMWLAN_SUCCESS: return "SUCCESS";
        case MMWLAN_ERROR: return "ERROR";
        case MMWLAN_INVALID_ARGUMENT: return "INVALID_ARGUMENT";
        case MMWLAN_UNAVAILABLE: return "UNAVAILABLE";
        case MMWLAN_CHANNEL_LIST_NOT_SET: return "CHANNEL_LIST_NOT_SET";
        case MMWLAN_NO_MEM: return "NO_MEM";
        case MMWLAN_TIMED_OUT: return "TIMED_OUT";
        case MMWLAN_SHUTDOWN_BLOCKED: return "SHUTDOWN_BLOCKED";
        case MMWLAN_CHANNEL_INVALID: return "CHANNEL_INVALID";
        case MMWLAN_NOT_FOUND: return "NOT_FOUND";
        case MMWLAN_NOT_RUNNING: return "NOT_RUNNING";
        default: return "UNKNOWN";
    }
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed.
 */
void app_main(void)
{
    printf("\n\nSoftAP Example (Built " __DATE__ " " __TIME__ ")\n\n");

    mmhal_init();
    mmwlan_init();

    const struct mmwlan_s1g_channel_list *channel_list = load_channel_list();
    printf("Loaded channel list for country: %s (%u channels)\n",
           channel_list->country_code, channel_list->num_channels);

    enum mmwlan_status status = mmwlan_set_channel_list(channel_list);
    if (status != MMWLAN_SUCCESS)
    {
        printf("mmwlan_set_channel_list failed: %s (%d)\n", status_to_str(status), status);
        MMOSAL_ASSERT(false);
    }

    printf("Calling mmwlan_boot(NULL)...\n");
    status = mmwlan_boot(NULL);
    if (status != MMWLAN_SUCCESS)
    {
        printf("mmwlan_boot failed: %s (%d)\n", status_to_str(status), status);
        MMOSAL_ASSERT(false);
    }

    app_print_version_info();

    struct mmipal_init_args mmipal_args = MMIPAL_INIT_ARGS_DEFAULT;
    mmipal_args.mode = MMIPAL_STATIC;
    mmipal_args.ip6_mode = MMIPAL_IP6_DISABLED;
    load_softap_mmipal_init_args(&mmipal_args);

    printf("Initializing network interface (IP: %s, Netmask: %s, Gateway: %s)\n",
           mmipal_args.ip_addr, mmipal_args.netmask, mmipal_args.gateway_addr);

    if (mmipal_init(&mmipal_args) != MMIPAL_SUCCESS)
    {
        printf("Error initializing network interface.\n");
        MMOSAL_ASSERT(false);
    }

    mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED);
    load_mmwlan_settings_softap();

    struct mmwlan_softap_args softap_args = MMWLAN_SOFTAP_ARGS_INIT;
    load_mmwlan_softap_args(&softap_args);

    softap_args.sta_status_cb = handle_softap_sta_status;
    softap_args.sta_status_cb_arg = NULL;

    printf("Starting Soft AP with:\n");
    printf("  SSID: %.*s\n", softap_args.ssid_len, softap_args.ssid);
    printf("  Security: %s\n", softap_args.security_type == MMWLAN_SAE ? "SAE" :
                              softap_args.security_type == MMWLAN_OWE ? "OWE" : "OPEN");
    printf("  PMF Mode: %s\n", softap_args.pmf_mode == MMWLAN_PMF_REQUIRED ? "REQUIRED" :
                               softap_args.pmf_mode == MMWLAN_PMF_DISABLED ? "DISABLED" : "UNKNOWN");
    if (softap_args.security_type == MMWLAN_SAE)
    {
        printf("  Passphrase length: %u\n", softap_args.passphrase_len);
    }
    printf("  Op Class: %u\n", softap_args.op_class);
    printf("  S1G Channel: %u\n", softap_args.s1g_chan_num);
    printf("  Primary BW: %u MHz\n", softap_args.pri_bw_mhz);
    printf("  Primary 1MHz Chan Idx: %u\n", softap_args.pri_1mhz_chan_idx);
    printf("  Beacon Interval: %u TUs\n", softap_args.beacon_interval_tus);
    printf("  DTIM Period: %u\n", softap_args.dtim_period);

    /* Validate that the channel exists in the loaded channel list */
    const struct mmwlan_s1g_channel_list *verify_list = load_channel_list();
    bool found = false;
    for (unsigned i = 0; i < verify_list->num_channels; i++)
    {
        if (verify_list->channels[i].s1g_chan_num == softap_args.s1g_chan_num)
        {
            printf("Found matching channel at index %u:\n", i);
            printf("  S1G Op Class in DB: %d\n", verify_list->channels[i].s1g_operating_class);
            printf("  Global Op Class in DB: %d\n", verify_list->channels[i].global_operating_class);
            printf("  BW in DB: %u MHz\n", verify_list->channels[i].bw_mhz);
            printf("  Freq in DB: %lu Hz\n", (unsigned long)verify_list->channels[i].centre_freq_hz);
            
            if (verify_list->channels[i].s1g_operating_class == softap_args.op_class ||
                verify_list->channels[i].global_operating_class == softap_args.op_class)
            {
                found = true;
                printf("  -> Op class MATCHES (using %s)\n",
                       verify_list->channels[i].s1g_operating_class == softap_args.op_class ? "S1G" : "Global");
                break;
            }
            else
            {
                printf("  -> Op class MISMATCH! (config=%u, DB S1G=%d, DB Global=%d)\n",
                       softap_args.op_class,
                       verify_list->channels[i].s1g_operating_class,
                       verify_list->channels[i].global_operating_class);
            }
        }
    }
    
    if (!found)
    {
        printf("WARNING: Channel %u with op_class %u not found in regulatory DB!\n",
               softap_args.s1g_chan_num, softap_args.op_class);
    }

    printf("\n*** IMPORTANT: SoftAP API is marked as BETA in mmwlan.h ***\n");
    printf("*** This feature may not be fully implemented in firmware 1.16.4 ***\n");
    printf("*** The mm6108 chip or this firmware version may not support SoftAP mode ***\n\n");
    
    status = mmwlan_softap_enable(&softap_args);
    if (status == MMWLAN_SUCCESS)
    {
        printf("Soft AP started successfully\n");
    }
    else
    {
        printf("Failed to start Soft AP (status %s / %d)\n",
               status_to_str(status), status);
        printf("\nPossible causes:\n");
        printf("  1. SoftAP is a BETA API and may not be fully functional\n");
        printf("  2. Firmware 1.16.4 may not support SoftAP on mm6108 hardware\n");
        printf("  3. The mm6108-mf08651-us module may be STA-only\n");
        printf("  4. Additional firmware configuration or licensing may be required\n");
        printf("\nRecommendation: Use STA (station) mode instead, or contact Morse Micro support.\n");
    }
}
