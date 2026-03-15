/*
 * Copyright 2025 Morse Micro
 *
 * This file is licensed under terms that can be found in the LICENSE.md file in the root
 * directory of the Morse Micro IoT SDK software package.
 */

/**
 * @file
 * @brief AP Mode Example Application.
 *
 * @note It is assumed that you have followed the steps in the @ref GETTING_STARTED guide and are
 * therefore familiar with how to build, flash, and monitor an application using the MM-IoT-SDK
 * framework.
 */

#include <string.h>
#include "mmhal.h"
#include "mmosal.h"
#include "mmutils.h"
#include "mmipal.h"
#include "mmregdb.h"

// #define COUNTRY_CODE "AU"
#ifndef COUNTRY_CODE
#error COUNTRY_CODE must be defined to the appropriate 2 character country code. \
       See mmregdb.c for valid options.
#endif

/*
 * --
 * Default Network configuration
 * --
 */

#ifndef STATIC_LOCAL_IP
/** Statically configured IP address. */
#define STATIC_LOCAL_IP "192.168.1.1"
#endif
#ifndef STATIC_GATEWAY
/** Statically configured gateway address. */
#define STATIC_GATEWAY "192.168.1.1"
#endif
#ifndef STATIC_NETMASK
/** Statically configured netmask. */
#define STATIC_NETMASK "255.255.255.0"
#endif

/*
 * --
 * Default SSID/Security configuration
 * --
 */

#ifndef AP_SSID
/** SSID of the AP. (Do not quote; it will be stringified.) */
#define AP_SSID MorseMicroIoT
#endif

#ifndef SAE_PASSPHRASE
/** Passphrase of the AP (ignored if security type is not SAE).
 *  (Do not quote; it will be stringified.) */
#define SAE_PASSPHRASE 12345678
#endif

/* Default security type  */
#ifndef SECURITY_TYPE
/** Security type (@see mmwlan_security_type). */
#define SECURITY_TYPE MMWLAN_SAE
#endif

/* Default PMF mode */
#ifndef PMF_MODE
/** Protected Management Frames (PMF) mode (@see mmwlan_pmf_mode). */
#define PMF_MODE MMWLAN_PMF_REQUIRED
#endif

/*
 * --
 * Default channel configuration
 * --
 */

#ifndef OP_CLASS
/**
 * Operating Class to use for AP Mode.
 * This together with S1G_CHANNEL must correspond to a channel in the regulatory database.
 */
#define OP_CLASS (25)
#endif

#ifndef S1G_CHANNEL
/**
 * S1G Channel to use for AP Mode.
 * This together with OP_CLASS must correspond to a channel in the regulatory database.
 */
#define S1G_CHANNEL (43)
#endif

#ifndef PRIMARY_BW_MHZ
/**
 * Primary Bandwidth to use for AP Mode.
 *
 * Valid values:
 * * 0 (auto)
 * * 1
 * * 2
 */
#define PRIMARY_BW_MHZ (0)
#endif

#ifndef PRIMARY_1MHZ_CHANNEL_INDEX
/** Primary 1 MHz Channel Index to use for AP Mode */
#define PRIMARY_1MHZ_CHANNEL_INDEX (0)
#endif

#ifndef MAX_STAS
/**
 * The maximum number of stations that can connect to the AP.
 * Must not be greater than @ref MMWLAN_AP_MAX_STAS_LIMIT.
 */
#define MAX_STAS MMWLAN_DEFAULT_AP_MAX_STAS
#endif

/** Stringify macro. Do not use directly; use @ref STRINGIFY(). */
#define _STRINGIFY(x) #x
/** Convert the content of the given macro to a string. */
#define STRINGIFY(x) _STRINGIFY(x)

/** A throw away variable for checking that the opaque argument is correct. */
uint32_t opaque_argument_value;

/**
 * Handler for AP Mode STA Status callback.
 *
 * @param sta_status    STA status information.
 * @param arg           Opaque argument that was provided when the callback was registered.
 */
static void handle_ap_sta_status(const struct mmwlan_ap_sta_status *sta_status, void *arg)
{
    MM_UNUSED(sta_status);

    /* Validate that the opaque argument received matches the value passed in. This is just for
     * testing purposes. */
    MMOSAL_ASSERT(arg == &opaque_argument_value);

    printf("STA status updated\n");
}

/**
 * Loads the provided structure with initialization parameters
 * read from config store.  If a specific parameter is not found then
 * default values are used.  Use this function to load defaults before
 * calling @c mmwlan_ap_enable().
 *
 * @param ap_args A pointer to the @c mmwlan_ap_args to return
 *                    the settings in.
 */
void load_mmwlan_ap_args(struct mmwlan_ap_args *ap_args)
{
    /* Load SSID */
    (void)mmosal_safer_strcpy((char *)ap_args->ssid,
                              STRINGIFY(AP_SSID),
                              sizeof(ap_args->ssid));
    ap_args->ssid_len = strlen((char *)ap_args->ssid);
    /* Load password */
    (void)mmosal_safer_strcpy(ap_args->passphrase,
                              STRINGIFY(SAE_PASSPHRASE),
                              sizeof(ap_args->passphrase));
    ap_args->passphrase_len = strlen(ap_args->passphrase);

    ap_args->security_type = SECURITY_TYPE;
    ap_args->pmf_mode = PMF_MODE;
    ap_args->op_class = OP_CLASS;
    ap_args->s1g_chan_num = S1G_CHANNEL;
    ap_args->pri_bw_mhz = PRIMARY_BW_MHZ;
    ap_args->pri_1mhz_chan_idx = PRIMARY_1MHZ_CHANNEL_INDEX;
}

void app_print_version_info(void)
{
    enum mmwlan_status status;
    struct mmwlan_version version = { 0 };
    struct mmwlan_bcf_metadata bcf_metadata = { 0 };

    printf("-----------------------------------\n");
    printf("  HW Version:              %s\n", CONFIG_IDF_TARGET);

    status = mmwlan_get_bcf_metadata(&bcf_metadata);
    if (status == MMWLAN_SUCCESS)
    {
        printf("  BCF API version:         %u.%u.%u\n",
               bcf_metadata.version.major,
               bcf_metadata.version.minor,
               bcf_metadata.version.patch);
        if (bcf_metadata.build_version[0] != '\0')
        {
            printf("  BCF build version:       %s\n", bcf_metadata.build_version);
        }
        if (bcf_metadata.board_desc[0] != '\0')
        {
            printf("  BCF board description:   %s\n", bcf_metadata.board_desc);
        }
    }
    else
    {
        printf("  !! BCF metadata retrival failed !!\n");
    }

    status = mmwlan_get_version(&version);
    if (status != MMWLAN_SUCCESS)
    {
        printf("  !! Error occured whilst retrieving version info !!\n");
    }
    printf("  Morselib version:        %s\n", version.morselib_version);
    printf("  Morse firmware version:  %s\n", version.morse_fw_version);
    printf("  Morse chip ID:           0x%04lx\n", version.morse_chip_id);
    printf("  Morse chip name:         %s\n", version.morse_chip_id_string);
    printf("-----------------------------------\n");

    MMOSAL_ASSERT(status == MMWLAN_SUCCESS);
}

/**
 * Link status callback
 *
 * @param link_status   Current link status
 */
static void link_status_callback(const struct mmipal_link_status *link_status)
{
    uint32_t time_ms = mmosal_get_time_ms();
    if (link_status->link_state == MMIPAL_LINK_UP)
    {
        printf("Link is up. Time: %lu ms", time_ms);
        printf(", IP: %s", link_status->ip_addr);
        printf(", Netmask: %s", link_status->netmask);
        printf(", Gateway: %s\n", link_status->gateway);
    }
    else
    {
        printf("Link is down. Time: %lu ms\n", time_ms);
    }
}

/**
 * Main entry point to the application. This will be invoked in a thread once operating system
 * and hardware initialization has completed. It may return, but it does not have to.
 */
void app_main(void)
{
    enum mmwlan_status status;
    const struct mmwlan_s1g_channel_list *channel_list;
    struct mmwlan_boot_args boot_args = MMWLAN_BOOT_ARGS_INIT;
    struct mmwlan_ap_args ap_args = MMWLAN_AP_ARGS_INIT;
    struct mmipal_init_args mmipal_init_args = MMIPAL_INIT_ARGS_DEFAULT;

    printf("\n\nAP Mode Example (Built " __DATE__ " " __TIME__ ")\n\n");
    mmhal_init();
    mmwlan_init();

    channel_list = mmwlan_lookup_regulatory_domain(get_regulatory_db(), COUNTRY_CODE);
    if (channel_list == NULL)
    {
        printf("Could not find specified regulatory domain matching country code %s\n",
               COUNTRY_CODE);
        MMOSAL_ASSERT(false);
    }

    status = mmwlan_set_channel_list(channel_list);
    if (status != MMWLAN_SUCCESS)
    {
        printf("Failed to set country code %s\n", channel_list->country_code);
        MMOSAL_ASSERT(false);
    }

    mmwlan_boot(&boot_args);
    app_print_version_info();

    mmipal_init_args.mode = MMIPAL_STATIC;
    mmipal_init_args.ip6_mode = MMIPAL_IP6_DISABLED;
    (void)mmosal_safer_strcpy(mmipal_init_args.ip_addr,
                              STATIC_LOCAL_IP,
                              sizeof(mmipal_init_args.ip_addr));
    (void)mmosal_safer_strcpy(mmipal_init_args.netmask,
                              STATIC_NETMASK,
                              sizeof(mmipal_init_args.netmask));
    (void)mmosal_safer_strcpy(mmipal_init_args.gateway_addr,
                              STATIC_GATEWAY,
                              sizeof(mmipal_init_args.gateway_addr));

    if (mmipal_init(&mmipal_init_args) != MMIPAL_SUCCESS)
    {
        printf("Error initializing network interface.\n");
        MMOSAL_ASSERT(false);
    }

    mmipal_set_link_status_callback(link_status_callback);

    mmwlan_set_power_save_mode(MMWLAN_PS_DISABLED);
    load_mmwlan_ap_args(&ap_args);

    ap_args.sta_status_cb = handle_ap_sta_status;
    ap_args.sta_status_cb_arg = &opaque_argument_value;

    ap_args.max_stas = MAX_STAS;

    status = mmwlan_ap_enable(&ap_args);
    if (status == MMWLAN_SUCCESS)
    {
        printf("AP Mode started successfully\n");
    }
    else
    {
        printf("Failed to start AP Mode (status %d)\n", status);
    }
}
