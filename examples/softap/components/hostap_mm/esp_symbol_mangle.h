/*
 * Copyright 2025 Morse Micro
 *
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief Symbol mangling to avoid conflicts with ESP-IDF's built-in hostap/wpa_supplicant
 * 
 * This header mangles hostap symbols that collide with ESP-IDF's wpa_supplicant component.
 * The mangling prefixes Morse Micro hostap symbols with 'mm_' to avoid linker conflicts.
 */

#pragma once

/* Mangle hostapd core functions */
#define hostapd_init mm_hostapd_init
#define hostapd_setup_interface mm_hostapd_setup_interface
#define hostapd_deinit mm_hostapd_deinit
#define hostapd_config_defaults mm_hostapd_config_defaults
#define hostapd_config_free mm_hostapd_config_free
#define hostapd_config_read mm_hostapd_config_read
#define hostapd_interface_init mm_hostapd_interface_init
#define hostapd_interface_deinit mm_hostapd_interface_deinit

/* Mangle AP configuration functions */
#define hostapd_set_freq mm_hostapd_set_freq
#define hostapd_set_oper_chwidth mm_hostapd_set_oper_chwidth
#define hostapd_set_oper_centr_freq_seg0_idx mm_hostapd_set_oper_centr_freq_seg0_idx
#define hostapd_set_oper_centr_freq_seg1_idx mm_hostapd_set_oper_centr_freq_seg1_idx
#define hostapd_config_tx_queue mm_hostapd_config_tx_queue
#define hostapd_parse_rates mm_hostapd_parse_rates
#define hostapd_get_hw_features mm_hostapd_get_hw_features

/* Mangle beacon/probe functions */
#define ieee802_11_set_beacon mm_ieee802_11_set_beacon
#define ieee802_11_set_beacons mm_ieee802_11_set_beacons
#define ieee802_11_update_beacons mm_ieee802_11_update_beacons
#define ieee802_11_build_ap_params mm_ieee802_11_build_ap_params

/* Mangle station management functions */
#define ap_sta_add mm_ap_sta_add
#define ap_sta_remove mm_ap_sta_remove
#define ap_free_sta mm_ap_free_sta
#define ap_get_sta mm_ap_get_sta
#define ap_sta_flags_txt mm_ap_sta_flags_txt
#define ap_sta_disconnect mm_ap_sta_disconnect
#define ap_sta_deauth_cb mm_ap_sta_deauth_cb
#define ap_sta_disassoc_cb mm_ap_sta_disassoc_cb
#define ap_sta_session_timeout mm_ap_sta_session_timeout
#define ap_sta_no_session_timeout mm_ap_sta_no_session_timeout

/* Mangle WPA authentication functions */
#define wpa_auth_sta_init mm_wpa_auth_sta_init
#define wpa_auth_sta_deinit mm_wpa_auth_sta_deinit
#define wpa_receive mm_wpa_receive
#define wpa_init mm_wpa_init
#define wpa_deinit mm_wpa_deinit
#define wpa_reconfig mm_wpa_reconfig
#define wpa_auth_pmksa_add mm_wpa_auth_pmksa_add
#define wpa_auth_pmksa_remove mm_wpa_auth_pmksa_remove

/* Mangle IEEE 802.11 management functions */
#define ieee802_11_mgmt mm_ieee802_11_mgmt
#define ieee802_11_send_deauth mm_ieee802_11_send_deauth
#define ieee802_11_send_disassoc mm_ieee802_11_send_disassoc
#define ieee802_11_rx_from_unknown mm_ieee802_11_rx_from_unknown

/* Mangle driver operation wrappers */
#define hostapd_drv_set_key mm_hostapd_drv_set_key
#define hostapd_drv_send_mlme mm_hostapd_drv_send_mlme
#define hostapd_drv_sta_deauth mm_hostapd_drv_sta_deauth
#define hostapd_drv_sta_disassoc mm_hostapd_drv_sta_disassoc
#define hostapd_drv_sta_remove mm_hostapd_drv_sta_remove
#define hostapd_drv_set_ap mm_hostapd_drv_set_ap
#define hostapd_drv_set_ssid mm_hostapd_drv_set_ssid
#define hostapd_drv_sta_add mm_hostapd_drv_sta_add

/* Mangle common utility functions that might conflict */
#define wpa_hexdump mm_wpa_hexdump
#define wpa_hexdump_key mm_wpa_hexdump_key
#define wpa_printf mm_wpa_printf
#define wpa_debug_open_file mm_wpa_debug_open_file
#define wpa_debug_close_file mm_wpa_debug_close_file
#define wpa_debug_level mm_wpa_debug_level

/* Mangle EAPOL functions */
#define ieee802_1x_receive mm_ieee802_1x_receive
#define ieee802_1x_new_station mm_ieee802_1x_new_station
#define ieee802_1x_free_station mm_ieee802_1x_free_station
#define ieee802_1x_alloc_eapol_sm mm_ieee802_1x_alloc_eapol_sm
#define ieee802_1x_init mm_ieee802_1x_init
#define ieee802_1x_deinit mm_ieee802_1x_deinit

/* Mangle RADIUS client functions */
#define radius_client_init mm_radius_client_init
#define radius_client_deinit mm_radius_client_deinit
#define radius_client_send mm_radius_client_send
#define radius_client_get_id mm_radius_client_get_id

/* Mangle accounting functions */
#define accounting_init mm_accounting_init
#define accounting_deinit mm_accounting_deinit
#define accounting_sta_start mm_accounting_sta_start
#define accounting_sta_stop mm_accounting_sta_stop

/* Mangle WMM/QoS functions */
#define hostapd_wmm_action mm_hostapd_wmm_action
#define wmm_ac_init mm_wmm_ac_init
#define wmm_ac_deinit mm_wmm_ac_deinit

/* Mangle control interface functions */
#define hostapd_ctrl_iface_init mm_hostapd_ctrl_iface_init
#define hostapd_ctrl_iface_deinit mm_hostapd_ctrl_iface_deinit

/* Mangle WPS functions */
#define hostapd_init_wps mm_hostapd_init_wps
#define hostapd_deinit_wps mm_hostapd_deinit_wps
#define hostapd_wps_eap_completed mm_hostapd_wps_eap_completed
