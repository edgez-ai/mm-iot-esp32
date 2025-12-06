/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Morse Micro load configuration helper for ESP32
 *
 * This file contains helper routines to load commonly used configuration settings.
 * On ESP32, configuration is compiled in via preprocessor defines rather than
 * being loaded from a config store.
 */

#include <string.h>

#include "mmwlan.h"
#include "mmipal.h"
#include "mmosal.h"
#include "mm_app_loadconfig.h"
#include "mm_app_regdb.h"

#ifndef COUNTRY_CODE
#define COUNTRY_CODE "??"
#endif

/** Stringify macro. Do not use directly; use @ref STRINGIFY(). */
#define _STRINGIFY(x) #x
/** Convert the content of the given macro to a string. */
#define STRINGIFY(x) _STRINGIFY(x)

/**
 * Remove a single pair of surrounding double quotes if present. This helps when COUNTRY_CODE
 * is provided as a quoted string (e.g. -DCOUNTRY_CODE="US").
 */
static void strip_wrapping_quotes(char *s)
{
    size_t len = strlen(s);
    if (len >= 2 && s[0] == '"' && s[len - 1] == '"')
    {
        memmove(s, s + 1, len - 1);
        s[len - 2] = '\0';
    }
}

const struct mmwlan_s1g_channel_list* load_channel_list(void)
{
    char strval[16];
    const struct mmwlan_s1g_channel_list *channel_list;

    /* Use the compiled-in country code */
    (void)mmosal_safer_strcpy(strval, STRINGIFY(COUNTRY_CODE), sizeof(strval));
    printf("Raw COUNTRY_CODE from build: '%s'\n", strval);
    strip_wrapping_quotes(strval);
    printf("After stripping quotes: '%s'\n", strval);
    
    channel_list = mmwlan_lookup_regulatory_domain(get_regulatory_db(), strval);

    if (channel_list == NULL)
    {
        printf("Could not find specified regulatory domain matching country code '%s'\n", strval);
        printf("Please set COUNTRY_CODE in your build configuration.\n");
        MMOSAL_ASSERT(false);
    }
    printf("Successfully loaded regulatory domain for '%s'\n", strval);
    
    /* Dump first 10 channels for debugging */
    printf("First 10 channels in list:\n");
    for (unsigned i = 0; i < channel_list->num_channels && i < 10; i++)
    {
        printf("  [%u] Chan=%u, GlobalOpClass=%d, S1GOpClass=%d, BW=%u MHz, Freq=%lu Hz\n",
               i, channel_list->channels[i].s1g_chan_num,
               channel_list->channels[i].global_operating_class,
               channel_list->channels[i].s1g_operating_class,
               channel_list->channels[i].bw_mhz,
               (unsigned long)channel_list->channels[i].centre_freq_hz);
    }
    
    return channel_list;
}

bool country_code_in_regulatory_domain(const char * code)
{
    if (mmwlan_lookup_regulatory_domain(get_regulatory_db(), code) == NULL)
    {
        return false;
    }
    return true;
}
