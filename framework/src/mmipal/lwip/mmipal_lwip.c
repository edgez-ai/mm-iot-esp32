/*
 * Copyright 2021-2023 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "mmipal.h"
#include "mmnetif.h"
#include "mmwlan.h"
#include "mmosal.h"
#include "mmutils.h"

#include <string.h>

#include "lwip/api.h"
#include "lwip/autoip.h"
#include "lwip/def.h"
#include "lwip/dhcp.h"
#include "lwip/dhcp6.h"
#include "lwip/dns.h"
#include "lwip/etharp.h"
#include "lwip/ethip6.h"
#include "lwip/igmp.h"
#include "lwip/inet.h"
#include "lwip/ip_addr.h"
#include "lwip/ip4_frag.h"
#include "lwip/ip6_frag.h"
#include "lwip/mem.h"
#include "lwip/sockets.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "lwip/tcpip.h"
#include "lwip/udp.h"

static struct mmipal_data
{
    struct netif lwip_mmnetif;
    /** This stores the IPv4 link state for the IP stack. I.e., do we have an IP address or not. */
    enum mmipal_link_state ip_link_state;
    /** Flag requesting ARP response offload feature */
    bool offload_arp_response;
    /** ARP refresh offload interval in seconds */
    uint32_t offload_arp_refresh_s;
    bool dhcp_offload_init_complete;
    /** The link status callback function that has been registered. */
    mmipal_link_status_cb_fn_t link_status_callback;
    /** The extended link status callback function that has been registered. */
    mmipal_ext_link_status_cb_fn_t ext_link_status_callback;
    /** Argument for the extended link status callback function that has been registered. */
    void *ext_link_status_callback_arg;
#if LWIP_IPV4
    enum mmipal_addr_mode ip4_mode;
#endif
#if LWIP_IPV6
    enum mmipal_ip6_addr_mode ip6_mode;
#endif
} mmipal_data = {};

static uint32_t s_mmipal_init_start_ms = 0;

#ifndef MMIPAL_TIMING_LOG
#define MMIPAL_TIMING_LOG 0
#endif

#if MMIPAL_TIMING_LOG
#define MMIPAL_TIMING_PRINTF(...) printf(__VA_ARGS__)
#else
#define MMIPAL_TIMING_PRINTF(...) do {} while (0)
#endif

static const char *mmipal_link_state_to_str(enum mmipal_link_state state)
{
    switch (state)
    {
    case MMIPAL_LINK_DOWN:
        return "DOWN";
    case MMIPAL_LINK_UP:
        return "UP";
    default:
        return "UNKNOWN";
    }
}

static void mmipal_log_netif_state(const char *reason, const struct netif *netif,
                                   enum mmipal_link_state ip_link_state)
{
    char ip[IPADDR_STRLEN_MAX] = {0};
    char netmask[IPADDR_STRLEN_MAX] = {0};
    char gateway[IPADDR_STRLEN_MAX] = {0};

    if (netif == NULL)
    {
        printf("mmipal: %s netif=NULL\n", reason);
        return;
    }

#if LWIP_IPV4
    unsigned dhcp_supplied = dhcp_supplied_address((struct netif *)netif) ? 1u : 0u;
    if (ipaddr_ntoa_r(&netif->ip_addr, ip, sizeof(ip)) == NULL)
    {
        strcpy(ip, "<err>");
    }
    if (ipaddr_ntoa_r(&netif->netmask, netmask, sizeof(netmask)) == NULL)
    {
        strcpy(netmask, "<err>");
    }
    if (ipaddr_ntoa_r(&netif->gw, gateway, sizeof(gateway)) == NULL)
    {
        strcpy(gateway, "<err>");
    }
#else
    unsigned dhcp_supplied = 0;
    strcpy(ip, "n/a");
    strcpy(netmask, "n/a");
    strcpy(gateway, "n/a");
#endif

    printf("mmipal: %s netif=%c%c%u flags=0x%02x link_up=%u netif_up=%u dhcp_supplied=%u ip_link_state=%s ip=%s nm=%s gw=%s\n",
           reason,
           netif->name[0],
           netif->name[1],
           netif->num,
           (unsigned)netif->flags,
           (unsigned)netif_is_link_up(netif),
           (unsigned)netif_is_up(netif),
           dhcp_supplied,
           mmipal_link_state_to_str(ip_link_state),
           ip,
           netmask,
           gateway);
}

/** Getter function to retrieve the global mmipal data structure.*/
static inline struct mmipal_data *mmipal_get_data(void)
{
    return &mmipal_data;
}

static void netif_status_callback(struct netif *netif);

#if LWIP_IPV4

/**
 * DHCP Lease update callback, invoked when we get a new DHCP lease.
 *
 * @param lease_info The new DHCP lease.
 */
static void mmipal_dhcp_lease_updated(const struct mmwlan_dhcp_lease_info *lease_info, void *arg)
{
    struct mmipal_data *data = mmipal_get_data();

    ip4_addr_t ip_addr, netmask, gateway;
    ip_addr_t dns_addr = ip_addr_any;

    MM_UNUSED(arg);

    data->dhcp_offload_init_complete = true;

    ip4_addr_set_u32(&ip_addr, lease_info->ip4_addr);
    ip4_addr_set_u32(&netmask, lease_info->mask4_addr);
    ip4_addr_set_u32(&gateway, lease_info->gw4_addr);
    ip4_addr_set_u32(ip_2_ip4(&dns_addr), lease_info->dns4_addr);

    {
        char ip_buf[IPADDR_STRLEN_MAX] = {0};
        char netmask_buf[IPADDR_STRLEN_MAX] = {0};
        char gateway_buf[IPADDR_STRLEN_MAX] = {0};
        char dns_buf[IPADDR_STRLEN_MAX] = {0};

        ip4addr_ntoa_r(&ip_addr, ip_buf, sizeof(ip_buf));
        ip4addr_ntoa_r(&netmask, netmask_buf, sizeof(netmask_buf));
        ip4addr_ntoa_r(&gateway, gateway_buf, sizeof(gateway_buf));
        ip4addr_ntoa_r(ip_2_ip4(&dns_addr), dns_buf, sizeof(dns_buf));

        printf("mmipal: DHCP offload lease updated ip=%s mask=%s gw=%s dns=%s\n",
               ip_buf, netmask_buf, gateway_buf, dns_buf);
    }

    LOCK_TCPIP_CORE();
    netif_set_addr(&data->lwip_mmnetif, &ip_addr, &netmask, &gateway);
    dns_setserver(0, &dns_addr);
    UNLOCK_TCPIP_CORE();

    netif_status_callback(&data->lwip_mmnetif);
}

enum mmipal_status mmipal_get_ip_config(struct mmipal_ip_config *config)
{
    struct mmipal_data *data = mmipal_get_data();
    char *result;

    config->mode = data->ip4_mode;

    result = ipaddr_ntoa_r(&data->lwip_mmnetif.ip_addr, config->ip_addr, sizeof(config->ip_addr));
    LWIP_ASSERT("IP buf too short", result != NULL);

    result = ipaddr_ntoa_r(&data->lwip_mmnetif.netmask, config->netmask, sizeof(config->netmask));
    LWIP_ASSERT("IP buf too short", result != NULL);

    result = ipaddr_ntoa_r(&data->lwip_mmnetif.gw, config->gateway_addr,
                           sizeof(config->gateway_addr));
    LWIP_ASSERT("IP buf too short", result != NULL);

    return MMIPAL_SUCCESS;
}

enum mmipal_status mmipal_set_ip_config(const struct mmipal_ip_config *config)
{
    struct mmipal_data *data = mmipal_get_data();
    int result;
    ip_addr_t ip_addr = ip_addr_any;
    ip_addr_t netmask = ip_addr_any;
    ip_addr_t gateway = ip_addr_any;
    struct netif *netif = &data->lwip_mmnetif;

    if (config->mode != MMIPAL_DHCP_OFFLOAD && data->ip4_mode == MMIPAL_DHCP_OFFLOAD)
    {
        printf("Once enabled DHCP offload mode cannot be disabled\n");
        return MMIPAL_NOT_SUPPORTED;
    }

    switch (config->mode)
    {
    case MMIPAL_DISABLED:
        printf("%s mode not supported\n", "DISABLED");
        return MMIPAL_INVALID_ARGUMENT;

    case MMIPAL_AUTOIP:
        printf("%s mode not supported\n", "AutoIP");
        return MMIPAL_INVALID_ARGUMENT;

    case MMIPAL_DHCP_OFFLOAD:
        /* Currently we only support enabling DHCP offload when initialising */
        printf("%s mode not supported\n", "DHCP_OFFLOAD");
        return MMIPAL_INVALID_ARGUMENT;

    case MMIPAL_STATIC:
        result = ipaddr_aton(config->ip_addr, &ip_addr);
        if (!result)
        {
            return MMIPAL_INVALID_ARGUMENT;
        }
        result = ipaddr_aton(config->netmask, &netmask);
        if (!result)
        {
            return MMIPAL_INVALID_ARGUMENT;
        }
        result = ipaddr_aton(config->gateway_addr, &gateway);
        if (!result)
        {
            return MMIPAL_INVALID_ARGUMENT;
        }
        break;

    case MMIPAL_DHCP:
        break;
    }

    LOCK_TCPIP_CORE();

        printf("mmipal: set_ip_config mode=%d ip=%s netmask=%s gw=%s (prev_mode=%d)\n",
            (int)config->mode,
            config->ip_addr,
            config->netmask,
            config->gateway_addr,
            (int)data->ip4_mode);

    if (config->mode != MMIPAL_DHCP && data->ip4_mode == MMIPAL_DHCP)
    {
        /* Stop DHCP if it was started earlier before setting static IP */
        dhcp_stop(netif);
    }

    data->ip4_mode = config->mode;

    netif_set_addr(netif, ip_2_ip4(&ip_addr), ip_2_ip4(&netmask), ip_2_ip4(&gateway));
    if (data->ip4_mode == MMIPAL_DHCP)
    {
        result = dhcp_start(netif);
        LWIP_ASSERT("DHCP start error", result == ERR_OK);
    }

    UNLOCK_TCPIP_CORE();
    mmipal_log_netif_state("after mmipal_set_ip_config", netif, data->ip_link_state);
    return MMIPAL_SUCCESS;
}

enum mmipal_status mmipal_get_ip_broadcast_addr(mmipal_ip_addr_t broadcast_addr)
{
    struct mmipal_data *data = mmipal_get_data();
    char *result;

    uint32_t ip_addr = ip_addr_get_ip4_u32(&data->lwip_mmnetif.ip_addr);
    uint32_t netmask = ip_addr_get_ip4_u32(&data->lwip_mmnetif.netmask);
    uint32_t broadcast_u32 = (ip_addr & netmask) | (0xffffffff & ~netmask);
    ip_addr_t broadcast_ip_addr;
    ip_addr_t *_broadcast_ip_addr = &broadcast_ip_addr;
    ip_addr_set_ip4_u32(_broadcast_ip_addr, broadcast_u32);

    result = ipaddr_ntoa_r(&broadcast_ip_addr, broadcast_addr, MMIPAL_IPADDR_STR_MAXLEN);
    LWIP_ASSERT("IP buf too short", result != NULL);

    return MMIPAL_SUCCESS;
}

#else
enum mmipal_status mmipal_get_ip_config(struct mmipal_ip_config *config)
{
    MM_UNUSED(config);
    LWIP_ASSERT("IPv4 not enabled", false);
    return MMIPAL_NOT_SUPPORTED;
}

enum mmipal_status mmipal_set_ip_config(const struct mmipal_ip_config *config)
{
    MM_UNUSED(config);
    LWIP_ASSERT("IPv4 not enabled", false);
    return MMIPAL_NOT_SUPPORTED;
}

enum mmipal_status mmipal_get_ip_broadcast_addr(mmipal_ip_addr_t broadcast_addr)
{
    MM_UNUSED(broadcast_addr);
    LWIP_ASSERT("IPv4 not enabled", false);
    return MMIPAL_NOT_SUPPORTED;
}

#endif

#if LWIP_IPV6

enum mmipal_status mmipal_get_ip6_config(struct mmipal_ip6_config *config)
{
    struct mmipal_data *data = mmipal_get_data();
    unsigned ii;
    struct netif *netif = &data->lwip_mmnetif;

    if (config == NULL)
    {
        return MMIPAL_INVALID_ARGUMENT;
    }
    config->ip6_mode = data->ip6_mode;
    for (ii = 0; ii < LWIP_IPV6_NUM_ADDRESSES; ii++)
    {
        char *result;
        const ip_addr_t *addr = &ip6_addr_any;

        if (ip6_addr_isvalid(netif_ip6_addr_state(netif, ii)))
        {
            addr = &data->lwip_mmnetif.ip6_addr[ii];
        }

        result = ipaddr_ntoa_r(addr, config->ip6_addr[ii], sizeof(config->ip6_addr[ii]));
        LWIP_ASSERT("IP buf too short", result != NULL);
    }
    return MMIPAL_SUCCESS;
}

enum mmipal_status mmipal_set_ip6_config(const struct mmipal_ip6_config *config)
{
    struct mmipal_data *data = mmipal_get_data();
    struct netif *netif = &data->lwip_mmnetif;
    err_t result;
    unsigned ii;
    ip_addr_t ip6_addr[LWIP_IPV6_NUM_ADDRESSES];

    for (ii = 0; ii < LWIP_IPV6_NUM_ADDRESSES; ii++)
    {
        int result = ipaddr_aton(config->ip6_addr[ii], &ip6_addr[ii]);
        if (!result)
        {
            return MMIPAL_INVALID_ARGUMENT;
        }
    }

    LOCK_TCPIP_CORE();

    if (config->ip6_mode == MMIPAL_IP6_STATIC)
    {
        if (data->ip6_mode != MMIPAL_IP6_STATIC)
        {
#if LWIP_IPV6_DHCP6
            dhcp6_disable(netif);
#endif
            netif_set_ip6_autoconfig_enabled(netif, 0);
            data->ip6_mode = MMIPAL_IP6_STATIC;
        }

        if (!ip6_addr_islinklocal(ip_2_ip6(&(ip6_addr[0]))))
        {
            printf("First address must be linklocal address (address start with fe80)\n");
        }

        for (ii = 0; ii < LWIP_IPV6_NUM_ADDRESSES; ii++)
        {
            if (ip_addr_isany_val(ip6_addr[ii]))
            {
                netif_ip6_addr_set(netif, ii, IP6_ADDR_ANY6);
                netif_ip6_addr_set_state(netif, ii, IP6_ADDR_INVALID);
            }
            else
            {
                netif_ip6_addr_set(netif, ii, ip_2_ip6(&(ip6_addr[ii])));
                netif_ip6_addr_set_state(netif, ii, IP6_ADDR_TENTATIVE);
                netif_ip6_addr_set_valid_life(netif, ii, IP6_ADDR_LIFE_STATIC);
            }
        }
    }
    else
    {
        if (data->ip6_mode == MMIPAL_IP6_STATIC)
        {
            for (ii = 0; ii < LWIP_IPV6_NUM_ADDRESSES; ii++)
            {
                netif_ip6_addr_set(netif, ii, IP6_ADDR_ANY6);
                netif_ip6_addr_set_state(netif, ii, IP6_ADDR_INVALID);
            }
        }
        netif_set_ip6_autoconfig_enabled(netif, 1);
        netif_create_ip6_linklocal_address(netif, 1);
        data->ip6_mode = MMIPAL_IP6_AUTOCONFIG;
    }

    if (config->ip6_mode == MMIPAL_IP6_DHCP6_STATELESS)
#if LWIP_IPV6_DHCP6
    {
        result = dhcp6_enable_stateless(netif);
        LWIP_ASSERT("Stateless DHCP6 start error", result == ERR_OK);
        data->ip6_mode = MMIPAL_IP6_DHCP6_STATELESS;
    }
    else
    {
        dhcp6_disable(netif);
    }
#else
    {
        printf("LWIP_IPV6_DHCP6 is not enabled\n");
    }
#endif

    UNLOCK_TCPIP_CORE();
    return MMIPAL_SUCCESS;
}

#else
enum mmipal_status mmipal_get_ip6_config(struct mmipal_ip6_config *config)
{
    MM_UNUSED(config);
    LWIP_ASSERT("IPv6 not enabled", false);
    return MMIPAL_NOT_SUPPORTED;
}

enum mmipal_status  mmipal_set_ip6_config(const struct mmipal_ip6_config *config)
{
    MM_UNUSED(config);
    LWIP_ASSERT("IPv6 not enabled", false);
    return MMIPAL_NOT_SUPPORTED;
}

#endif

static bool mmipal_link_status_check(struct netif *netif)
{
    bool ip4_addr_check = true;

#if LWIP_IPV4
    ip4_addr_check = !ip_addr_isany(&(netif->ip_addr));
#endif

    return ip4_addr_check && netif_is_link_up(netif);
}

/** Handler for @c netif status callbacks from LWIP. */
static void netif_status_callback(struct netif *netif)
{
    struct mmipal_data *data = mmipal_get_data();
    enum mmipal_link_state new_link_state = MMIPAL_LINK_DOWN;
#if MMIPAL_TIMING_LOG
    uint32_t now_ms = mmosal_get_time_ms();
#endif

#if LWIP_IPV4
    if (data->ip4_mode == MMIPAL_DHCP_OFFLOAD)
    {
        /* Initialize DHCP offload on link up */
        if (mmwlan_enable_dhcp_offload(mmipal_dhcp_lease_updated, NULL) != MMWLAN_SUCCESS)
        {
            printf("Failed to enable DHCP offload!\n");
        }

        if (!data->dhcp_offload_init_complete)
        {
            /* This just prevents a spurious 'Link Up' message on very first call */
            return;
        }
    }
#endif

    if (mmipal_link_status_check(netif))
    {
        new_link_state = MMIPAL_LINK_UP;
    }

    mmipal_log_netif_state("netif_status_callback", netif, new_link_state);

    if (data->ip_link_state != new_link_state)
    {
        data->ip_link_state = new_link_state;
        MMIPAL_TIMING_PRINTF("mmipal_timing: link_state=%s at %lu ms (since mmipal_init start: %lu ms)\n",
                     (new_link_state == MMIPAL_LINK_UP) ? "UP" : "DOWN",
                     (unsigned long)now_ms,
                     (unsigned long)(now_ms - s_mmipal_init_start_ms));

        if (data->link_status_callback || data->ext_link_status_callback)
        {
            struct mmipal_link_status link_status;
            memset(&link_status, 0, sizeof(link_status));
            link_status.link_state = data->ip_link_state;

#if LWIP_IPV4
            char *result = ipaddr_ntoa_r(&netif->ip_addr,
                                         link_status.ip_addr, sizeof(link_status.ip_addr));
            LWIP_ASSERT("IP buf too short", result != NULL);

            result = ipaddr_ntoa_r(&netif->netmask,
                                   link_status.netmask, sizeof(link_status.netmask));
            LWIP_ASSERT("IP buf too short", result != NULL);

            result = ipaddr_ntoa_r(&netif->gw,
                                   link_status.gateway, sizeof(link_status.gateway));
            LWIP_ASSERT("IP buf too short", result != NULL);

            if (data->ip_link_state == MMIPAL_LINK_UP)
            {
                /* Check if ARP response offload feature is enabled */
                if (data->offload_arp_response)
                {
                    mmwlan_enable_arp_response_offload(ip4_addr_get_u32(netif_ip4_addr(netif)));
                }

                /* Check if ARP refresh offload feature is enabled */
                if (data->offload_arp_refresh_s > 0)
                {
                    mmwlan_enable_arp_refresh_offload(data->offload_arp_refresh_s,
                                                      ip4_addr_get_u32(netif_ip4_gw(netif)), true);
                }
            }
#endif
            if (data->link_status_callback)
            {
                data->link_status_callback(&link_status);
            }
            if (data->ext_link_status_callback)
            {
                data->ext_link_status_callback(&link_status, data->ext_link_status_callback_arg);
            }
        }
    }
}

void mmipal_set_link_status_callback(mmipal_link_status_cb_fn_t fn)
{
    struct mmipal_data *data = mmipal_get_data();
    data->link_status_callback = fn;
}

void mmipal_set_ext_link_status_callback(mmipal_ext_link_status_cb_fn_t fn, void *arg)
{
    struct mmipal_data *data = mmipal_get_data();
    data->ext_link_status_callback = fn;
    data->ext_link_status_callback_arg = arg;
}

enum mmipal_status mmipal_rehook_lwip_callbacks(void)
{
    struct mmipal_data *data = mmipal_get_data();
    struct netif *netif = &data->lwip_mmnetif;

    if ((netif->name[0] == '\0') && (netif->name[1] == '\0'))
    {
        printf("mmipal: cannot rehook callbacks - netif not initialized\n");
        return MMIPAL_NO_LINK;
    }

    enum mmwlan_status wlan_status = mmnetif_register_callbacks(netif);
    if (wlan_status != MMWLAN_SUCCESS)
    {
        printf("mmipal: failed to rehook mmnetif callbacks: %d\n", (int)wlan_status);
        return MMIPAL_NO_LINK;
    }

    LOCK_TCPIP_CORE();
    netif_set_default(netif);
    netif_set_link_callback(netif, netif_status_callback);
    netif_set_status_callback(netif, netif_status_callback);
    UNLOCK_TCPIP_CORE();

    mmipal_log_netif_state("after mmipal_rehook_lwip_callbacks", netif, data->ip_link_state);
    return MMIPAL_SUCCESS;
}

static volatile bool tcpip_init_done = false;

struct lwip_init_args
{
    enum mmipal_addr_mode mode;
    enum mmipal_ip6_addr_mode ip6_mode;
    ip_addr_t ip_addr;
    ip_addr_t netmask;
    ip_addr_t gateway_addr;
    ip_addr_t ip6_addr;
};

struct mmipal_existing_lwip_attach_ctx
{
    struct lwip_init_args args;
    enum mmipal_status status;
};

static void mmipal_attach_existing_lwip_handler(void *arg)
{
    struct mmipal_existing_lwip_attach_ctx *ctx =
        (struct mmipal_existing_lwip_attach_ctx *)arg;
    struct mmipal_data *data = mmipal_get_data();
    struct netif *netif = &data->lwip_mmnetif;

    if (ctx == NULL)
    {
        return;
    }

    ctx->status = MMIPAL_NO_LINK;

    if ((netif->name[0] == '\0') && (netif->name[1] == '\0'))
    {
        struct netif *added = netif_add_noaddr(netif, NULL, mmnetif_init, tcpip_input);
        if (added == NULL)
        {
            printf("mmipal: netif_add_noaddr failed when attaching to existing lwip\n");
            return;
        }

        netif_set_default(netif);
        netif_set_up(netif);
    }

#if LWIP_IPV4
    data->ip4_mode = ctx->args.mode;
    if (ctx->args.mode == MMIPAL_DHCP)
    {
        err_t dhcp_err = dhcp_start(netif);
        if (dhcp_err != ERR_OK)
        {
            printf("mmipal: dhcp_start failed when attaching to existing lwip: %d\n",
                   (int)dhcp_err);
            return;
        }
    }
    else if (ctx->args.mode == MMIPAL_STATIC)
    {
        netif_set_addr(netif, ip_2_ip4(&(ctx->args.ip_addr)),
                       ip_2_ip4(&(ctx->args.netmask)), ip_2_ip4(&(ctx->args.gateway_addr)));
    }
#endif

    netif_set_link_callback(netif, netif_status_callback);
    netif_set_status_callback(netif, netif_status_callback);

#if LWIP_IPV6
    data->ip6_mode = ctx->args.ip6_mode;
    if (ctx->args.ip6_mode == MMIPAL_IP6_STATIC)
    {
        netif_ip6_addr_set(netif, 0, ip_2_ip6(&(ctx->args.ip6_addr)));
        netif_ip6_addr_set_state(netif, 0, IP6_ADDR_TENTATIVE);
    }
    else if (ctx->args.ip6_mode == MMIPAL_IP6_AUTOCONFIG)
    {
        netif_set_ip6_autoconfig_enabled(netif, 1);
        netif_create_ip6_linklocal_address(netif, 1);
    }
#if LWIP_IPV6_DHCP6
    else if (ctx->args.ip6_mode == MMIPAL_IP6_DHCP6_STATELESS)
    {
        err_t result6 = dhcp6_enable_stateless(netif);
        if (result6 != ERR_OK)
        {
            printf("mmipal: dhcp6_enable_stateless failed when attaching to existing lwip: %d\n",
                   (int)result6);
            return;
        }
    }
#endif
#endif

    mmipal_log_netif_state("after mmipal_init_on_existing_lwip", netif, data->ip_link_state);
    ctx->status = MMIPAL_SUCCESS;
}

enum mmipal_status mmipal_init_on_existing_lwip(const struct mmipal_init_args *args)
{
    struct mmipal_data *data = mmipal_get_data();
    int result;

    if (args == NULL)
    {
        return MMIPAL_INVALID_ARGUMENT;
    }

    struct mmipal_existing_lwip_attach_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.args.mode = args->mode;
    ctx.args.ip6_mode = args->ip6_mode;

    data->offload_arp_response = args->offload_arp_response;
    data->offload_arp_refresh_s = args->offload_arp_refresh_s;

#if LWIP_IPV4
    switch (args->mode)
    {
    case MMIPAL_DISABLED:
        return MMIPAL_INVALID_ARGUMENT;

    case MMIPAL_DHCP_OFFLOAD:
    case MMIPAL_STATIC:
        result = ipaddr_aton(args->ip_addr, &ctx.args.ip_addr);
        if (!result)
        {
            return MMIPAL_INVALID_ARGUMENT;
        }
        result = ipaddr_aton(args->netmask, &ctx.args.netmask);
        if (!result)
        {
            return MMIPAL_INVALID_ARGUMENT;
        }
        result = ipaddr_aton(args->gateway_addr, &ctx.args.gateway_addr);
        if (!result)
        {
            return MMIPAL_INVALID_ARGUMENT;
        }

        if (ip_addr_isany_val(ctx.args.ip_addr))
        {
            return MMIPAL_INVALID_ARGUMENT;
        }
        break;

    case MMIPAL_DHCP:
        if (LWIP_DHCP == 0)
        {
            return MMIPAL_NOT_SUPPORTED;
        }
        break;

    case MMIPAL_AUTOIP:
        return MMIPAL_INVALID_ARGUMENT;
    }
#endif

#if LWIP_IPV6
    switch (args->ip6_mode)
    {
    case MMIPAL_IP6_DISABLED:
        break;

    case MMIPAL_IP6_STATIC:
        result = ipaddr_aton(args->ip6_addr, &ctx.args.ip6_addr);
        if (!result)
        {
            return MMIPAL_INVALID_ARGUMENT;
        }
        if (ip_addr_isany_val(ctx.args.ip6_addr))
        {
            return MMIPAL_INVALID_ARGUMENT;
        }
        break;

    case MMIPAL_IP6_AUTOCONFIG:
        if (LWIP_IPV6_AUTOCONFIG == 0)
        {
            return MMIPAL_NOT_SUPPORTED;
        }
        break;

    case MMIPAL_IP6_DHCP6_STATELESS:
        if (LWIP_IPV6_DHCP6_STATELESS == 0)
        {
            return MMIPAL_NOT_SUPPORTED;
        }
        break;
    }
#endif

    err_t cb_err = tcpip_callback_with_block(mmipal_attach_existing_lwip_handler, &ctx, 1);
    if (cb_err != ERR_OK)
    {
        printf("mmipal: tcpip_callback_with_block failed in mmipal_init_on_existing_lwip: %d\n",
               (int)cb_err);
        return MMIPAL_NO_LINK;
    }

    return ctx.status;
}

static void tcpip_init_done_handler(void *arg)
{
    struct mmipal_data *data = mmipal_get_data();
    struct netif *netif = &data->lwip_mmnetif;
    struct lwip_init_args *args = (struct lwip_init_args *)arg;
#if MMIPAL_TIMING_LOG
    uint32_t cb_start_ms = mmosal_get_time_ms();
#endif

    MMIPAL_TIMING_PRINTF("mmipal_timing: tcpip_init_done_handler start at %lu ms (+%lu ms from mmipal_init)\n",
                         (unsigned long)cb_start_ms,
                         (unsigned long)(cb_start_ms - s_mmipal_init_start_ms));

    netif_add_noaddr(netif, NULL, mmnetif_init, tcpip_input);
    MMIPAL_TIMING_PRINTF("mmipal_timing: netif_add_noaddr done in %lu ms\n",
                         (unsigned long)(mmosal_get_time_ms() - cb_start_ms));
    netif_set_default(netif);
    netif_set_up(netif);
    mmipal_log_netif_state("after netif_set_up", netif, data->ip_link_state);

#if LWIP_IPV4
    err_t result;
    data->ip4_mode = args->mode;
    if (args->mode == MMIPAL_DHCP)
    {
        result = dhcp_start(netif);
        LWIP_ASSERT("DHCP start error", result == ERR_OK);
        printf("mmipal: DHCP start requested on netif %c%c%u\n", netif->name[0], netif->name[1], netif->num);
    }
    else if (args->mode == MMIPAL_STATIC)
    {
        netif_set_addr(netif, ip_2_ip4(&(args->ip_addr)),
                       ip_2_ip4(&(args->netmask)), ip_2_ip4(&(args->gateway_addr)));
        mmipal_log_netif_state("after static IPv4 set", netif, data->ip_link_state);
    }
#endif

    netif_set_link_callback(netif, netif_status_callback);
    netif_set_status_callback(netif, netif_status_callback);

#if LWIP_IPV6
    err_t result6;
    data->ip6_mode = args->ip6_mode;
    if (args->ip6_mode == MMIPAL_IP6_STATIC)
    {
        netif_ip6_addr_set(netif, 0, ip_2_ip6(&(args->ip6_addr)));
        netif_ip6_addr_set_state(netif, 0, IP6_ADDR_TENTATIVE);
    }
    else if (data->ip6_mode == MMIPAL_IP6_AUTOCONFIG)
    {
        netif_set_ip6_autoconfig_enabled(netif, 1);
        netif_create_ip6_linklocal_address(netif, 1);
    }
    else if (data->ip6_mode == MMIPAL_IP6_DHCP6_STATELESS)
#if LWIP_IPV6_DHCP6
    {
        result6 = dhcp6_enable_stateless(netif);
        LWIP_ASSERT("Stateless DHCP6 start error", result6 == ERR_OK);
    }
#else
    {
        printf("LWIP_IPV6_DHCP6 is not enabled\n");
    }
#endif
#endif

    mmosal_free(args);

    tcpip_init_done = true;
    MMIPAL_TIMING_PRINTF("mmipal_timing: tcpip_init_done_handler complete in %lu ms (total +%lu ms from mmipal_init)\n",
                         (unsigned long)(mmosal_get_time_ms() - cb_start_ms),
                         (unsigned long)(mmosal_get_time_ms() - s_mmipal_init_start_ms));
}

enum mmipal_status mmipal_init(const struct mmipal_init_args *args)
{
    struct mmipal_data *data = mmipal_get_data();
    enum mmipal_status status = MMIPAL_INVALID_ARGUMENT;
    int result;

    struct lwip_init_args *lwip_args = (struct lwip_init_args *)mmosal_malloc(sizeof(*lwip_args));
    s_mmipal_init_start_ms = mmosal_get_time_ms();
    MMIPAL_TIMING_PRINTF("mmipal_timing: mmipal_init start at %lu ms (mode=%d ip6_mode=%d)\n",
                         (unsigned long)s_mmipal_init_start_ms,
                         (int)args->mode,
                         (int)args->ip6_mode);
    if (lwip_args == NULL)
    {
        printf("malloc failure\n");
        return MMIPAL_NO_MEM;
    }

    memset(lwip_args, 0, sizeof(*lwip_args));
    lwip_args->mode = args->mode;
    lwip_args->ip6_mode = args->ip6_mode;

        printf("mmipal: init args mode=%d ip=%s netmask=%s gw=%s ip6_mode=%d ip6=%s offload_arp_response=%u offload_arp_refresh_s=%lu\n",
            (int)args->mode,
            args->ip_addr,
            args->netmask,
            args->gateway_addr,
            (int)args->ip6_mode,
            args->ip6_addr,
            (unsigned)args->offload_arp_response,
            (unsigned long)args->offload_arp_refresh_s);

    data->link_status_callback = NULL;

    data->offload_arp_response = args->offload_arp_response;
    data->offload_arp_refresh_s = args->offload_arp_refresh_s;

    /* Validate arguments */
#if LWIP_IPV4
    switch (args->mode)
    {
    case MMIPAL_DISABLED:
        printf("%s mode not supported\n", "DISABLED");
        goto exit;

    case MMIPAL_DHCP_OFFLOAD:
    case MMIPAL_STATIC:
        result = ipaddr_aton(args->ip_addr, &lwip_args->ip_addr);
        if (!result)
        {
            goto exit;
        }
        result = ipaddr_aton(args->netmask, &lwip_args->netmask);
        if (!result)
        {
            goto exit;
        }
        result = ipaddr_aton(args->gateway_addr, &lwip_args->gateway_addr);
        if (!result)
        {
            goto exit;
        }

        if (ip_addr_isany_val(lwip_args->ip_addr))
        {
            printf("IP address not specified\n");
            goto exit;
        }
        break;

    case MMIPAL_DHCP:
        if (LWIP_DHCP == 0)
        {
            printf("DHCP not compiled in\n");
            goto exit;
        }
        break;

    case MMIPAL_AUTOIP:
        printf("%s mode not supported\n", "AutoIP");
        break;
    }
#endif

#if LWIP_IPV6
    switch (args->ip6_mode)
    {
    case MMIPAL_IP6_DISABLED:
        break;

    case MMIPAL_IP6_STATIC:
        result = ipaddr_aton(args->ip6_addr, &lwip_args->ip6_addr);
        if (!result)
        {
            goto exit;
        }

        if (ip_addr_isany_val(lwip_args->ip6_addr))
        {
            printf("IP address not specified\n");
            goto exit;
        }
        break;

    case MMIPAL_IP6_AUTOCONFIG:
        if (LWIP_IPV6_AUTOCONFIG == 0)
        {
            printf("AUTOCONFIG not compiled in\n");
            goto exit;
        }
        break;

    case MMIPAL_IP6_DHCP6_STATELESS:
        if (LWIP_IPV6_DHCP6_STATELESS == 0)
        {
            printf("DHCP6_STATELESS not compiled in\n");
            goto exit;
        }
        break;
    }
#endif

    tcpip_init(tcpip_init_done_handler, lwip_args);
        MMIPAL_TIMING_PRINTF("mmipal_timing: tcpip_init invoked at +%lu ms\n",
                             (unsigned long)(mmosal_get_time_ms() - s_mmipal_init_start_ms));

    /* Block until initialisation is complete */
    while (!tcpip_init_done)
    {
        mmosal_task_sleep(10);
    }

        MMIPAL_TIMING_PRINTF("mmipal_timing: mmipal_init complete in %lu ms\n",
                             (unsigned long)(mmosal_get_time_ms() - s_mmipal_init_start_ms));

    return MMIPAL_SUCCESS;

exit:
    mmosal_free(lwip_args);
    return status;
}

void mmipal_get_link_packet_counts(uint32_t *tx_packets, uint32_t *rx_packets)
{
#if LWIP_STATS
    *tx_packets = lwip_stats.link.xmit;
    *rx_packets = lwip_stats.link.recv;
#else
    *tx_packets = 0;
    *rx_packets = 0;
#endif
}

void mmipal_set_tx_qos_tid(uint8_t tid)
{
    struct mmipal_data *data = mmipal_get_data();
    bool ok = tcpip_init_done;
    MMOSAL_ASSERT(ok);
    mmnetif_set_tx_qos_tid(&data->lwip_mmnetif, tid);
}

enum mmipal_link_state mmipal_get_link_state(void)
{
    struct mmipal_data *data = mmipal_get_data();
    return data->ip_link_state;
}

static enum mmipal_status mmipal_get_local_addr_(ip_addr_t *local_addr, const ip_addr_t *dest_addr)
{
    struct mmipal_data *data = mmipal_get_data();
    struct netif *netif = &data->lwip_mmnetif;
#if LWIP_IPV6
    if (IP_IS_V6(dest_addr))
    {
        const ip_addr_t *src_addr = ip6_select_source_address(netif, ip_2_ip6(dest_addr));
        if (src_addr == NULL)
        {
            printf("mmipal: no IPv6 local source address for destination\n");
            return MMIPAL_NO_LINK;
        }
        ip_addr_copy(*local_addr, *src_addr);
        return MMIPAL_SUCCESS;
    }
    else
    {
        MM_UNUSED(dest_addr);
    }
#endif

#if LWIP_IPV4
    if (IP_IS_V4(dest_addr))
    {
        if (ip_addr_isany_val(netif->ip_addr))
        {
            printf("mmipal: IPv4 local address not ready yet (netif ip is 0.0.0.0)\n");
        }
        ip_addr_copy(*local_addr, netif->ip_addr);
        return MMIPAL_SUCCESS;
    }
    else
    {
        MM_UNUSED(dest_addr);
    }
#endif

#if !LWIP_IPV4 && !LWIP_IPV6
    MM_UNUSED(local_addr);
    MM_UNUSED(dest_addr);
#endif

    return MMIPAL_INVALID_ARGUMENT;
}

enum mmipal_status mmipal_get_local_addr(mmipal_ip_addr_t local_addr,
                                         const mmipal_ip_addr_t dest_addr)
{
    ip_addr_t lwip_dest_addr;
    ip_addr_t lwip_local_addr;
    int ok;
    enum mmipal_status status;

    if (dest_addr == NULL)
    {
        return MMIPAL_INVALID_ARGUMENT;
    }

    ok = ipaddr_aton(dest_addr, &lwip_dest_addr);
    if (!ok)
    {
        return MMIPAL_INVALID_ARGUMENT;
    }

    status = mmipal_get_local_addr_(&lwip_local_addr, &lwip_dest_addr);
    if (status != 0)
    {
        return status;
    }

    if (ipaddr_ntoa_r(&lwip_local_addr, local_addr, MMIPAL_IPADDR_STR_MAXLEN) == NULL)
    {
        return MMIPAL_NO_MEM;
    }
    else
    {
        return MMIPAL_SUCCESS;
    }
}

enum mmipal_status mmipal_set_dns_server(uint8_t index, const mmipal_ip_addr_t addr)
{
    ip_addr_t dns_addr;
    int ok;

    if (index >= DNS_MAX_SERVERS)
    {
        return MMIPAL_INVALID_ARGUMENT;
    }

    ok = ipaddr_aton(addr, &dns_addr);
    if (!ok)
    {
        return MMIPAL_INVALID_ARGUMENT;
    }

    dns_setserver(index, &dns_addr);
    return MMIPAL_SUCCESS;
}

enum mmipal_status mmipal_get_dns_server(uint8_t index, mmipal_ip_addr_t addr)
{
    const ip_addr_t *dns_addr;

    if (index >= DNS_MAX_SERVERS)
    {
        return MMIPAL_INVALID_ARGUMENT;
    }

    dns_addr = dns_getserver(index);

#if LWIP_IPV4
    /* dns_getserver() returns ip_addr_any if no address configured. */
    if (!memcmp(dns_addr, &ip_addr_any, sizeof(*dns_addr)))
    {
        addr[0] = '\0';
        return MMIPAL_SUCCESS;
    }
#endif

    if (ipaddr_ntoa_r(dns_addr, addr, MMIPAL_IPADDR_STR_MAXLEN) == NULL)
    {
        return MMIPAL_NO_MEM;
    }
    else
    {
        return MMIPAL_SUCCESS;
    }
}

