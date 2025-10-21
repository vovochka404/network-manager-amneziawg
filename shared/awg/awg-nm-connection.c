/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include "awg-nm-connection.h"
#include "awg-device.h"
#include <netinet/in.h>
#include <string.h>

static void
add_ip_address(NMSettingIPConfig *s_ip, GInetAddress *address, guint32 prefix)
{
    NMIPAddress *addr;
    GError *error = NULL;

    addr = nm_ip_address_new(g_inet_address_get_family(address) == G_SOCKET_FAMILY_IPV4
                                 ? AF_INET
                                 : AF_INET6,
                             g_inet_address_to_string(address),
                             prefix,
                             &error);
    if (error) {
        g_warning("Failed to create IP address: %s", error->message);
        g_error_free(error);
        return;
    }

    nm_setting_ip_config_add_address(s_ip, addr);
    nm_ip_address_unref(addr);
}

gboolean
awg_device_save_to_nm_connection(AWGDevice *device, NMConnection *connection, GError **error)
{
    NMSettingConnection *s_con;
    NMSettingIP4Config *s_ip4;
    NMSettingIP6Config *s_ip6;
    NMSettingVpn *s_vpn;
    const gchar *value;
    const GInetAddress *addr;
    const GList *peers;
    guint16 port;
    guint16 fw_mark;
    guint i;

    g_return_val_if_fail(AWG_IS_DEVICE(device), FALSE);
    g_return_val_if_fail(NM_IS_CONNECTION(connection), FALSE);

    s_con = nm_connection_get_setting_connection(connection);
    if (!s_con) {
        s_con = NM_SETTING_CONNECTION(nm_setting_connection_new());
        nm_connection_add_setting(connection, NM_SETTING(s_con));
    }

    s_ip4 = NM_SETTING_IP4_CONFIG(nm_connection_get_setting(connection, NM_TYPE_SETTING_IP4_CONFIG));
    if (!s_ip4) {
        s_ip4 = NM_SETTING_IP4_CONFIG(nm_setting_ip4_config_new());
        nm_connection_add_setting(connection, NM_SETTING(s_ip4));
    }

    s_ip6 = NM_SETTING_IP6_CONFIG(nm_connection_get_setting(connection, NM_TYPE_SETTING_IP6_CONFIG));
    if (!s_ip6) {
        s_ip6 = NM_SETTING_IP6_CONFIG(nm_setting_ip6_config_new());
        nm_connection_add_setting(connection, NM_SETTING(s_ip6));
    }

    const char *old_uuid = nm_setting_connection_get_uuid(s_con);
    if (!old_uuid || !*old_uuid) {
        g_object_set(s_con,
                     NM_SETTING_CONNECTION_TYPE,
                     NM_SETTING_VPN_SETTING_NAME,
                     NM_SETTING_CONNECTION_UUID,
                     nm_utils_uuid_generate(),
                     NULL);
    }

    const char *old_id = nm_setting_connection_get_id(s_con);
    if (!old_id || !*old_id) {
        g_object_set(s_con, NM_SETTING_CONNECTION_ID, "AmneziaWG VPN", NULL);
    }

    addr = awg_device_get_address_v4(device);
    if (addr) {
        g_object_set(s_ip4,
                     NM_SETTING_IP_CONFIG_METHOD,
                     NM_SETTING_IP4_CONFIG_METHOD_MANUAL,
                     NULL);
        add_ip_address(NM_SETTING_IP_CONFIG(s_ip4), (GInetAddress *)addr, 32);
    } else {
        g_object_set(s_ip4, NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP4_CONFIG_METHOD_DISABLED, NULL);
    }

    addr = awg_device_get_address_v6(device);
    if (addr) {
        g_object_set(s_ip6,
                     NM_SETTING_IP_CONFIG_METHOD,
                     NM_SETTING_IP6_CONFIG_METHOD_MANUAL,
                     NULL);
        add_ip_address(NM_SETTING_IP_CONFIG(s_ip6), (GInetAddress *)addr, 128);
    } else {
        g_object_set(s_ip6, NM_SETTING_IP_CONFIG_METHOD, NM_SETTING_IP6_CONFIG_METHOD_IGNORE, NULL);
    }

    s_vpn = NM_SETTING_VPN(nm_setting_vpn_new());
    g_object_set(s_vpn, NM_SETTING_VPN_SERVICE_TYPE, NM_AWG_VPN_SERVICE_TYPE, NULL);
    nm_connection_add_setting(connection, NM_SETTING(s_vpn));

    value = awg_device_get_private_key(device);
    if (value) {
        nm_setting_vpn_add_secret(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY, value);
    }
    nm_setting_set_secret_flags(NM_SETTING(s_vpn),
                                NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY,
                                awg_device_get_private_key_flags(device),
                                NULL);

    value = awg_device_get_public_key(device);
    if (value) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PUBLIC_KEY, value);
    }

    port = awg_device_get_listen_port(device);
    if (port > 0) {
        g_autofree gchar *port_str = g_strdup_printf("%u", port);
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_LISTEN_PORT, port_str);
    }

    fw_mark = awg_device_get_fw_mark(device);
    if (fw_mark > 0) {
        g_autofree gchar *fw_mark_str = g_strdup_printf("%u", fw_mark);
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_FW_MARK, fw_mark_str);
    }

    {
        g_autofree gchar *jc_str = g_strdup_printf("%u", awg_device_get_jc(device));
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_JC, jc_str);
    }
    {
        g_autofree gchar *jmin_str = g_strdup_printf("%u", awg_device_get_jmin(device));
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_JMIN, jmin_str);
    }
    {
        g_autofree gchar *jmax_str = g_strdup_printf("%u", awg_device_get_jmax(device));
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_JMAX, jmax_str);
    }
    {
        g_autofree gchar *s1_str = g_strdup_printf("%u", awg_device_get_s1(device));
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_S1, s1_str);
    }
    {
        g_autofree gchar *s2_str = g_strdup_printf("%u", awg_device_get_s2(device));
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_S2, s2_str);
    }
    {
        g_autofree gchar *h1_str = g_strdup_printf("%u", awg_device_get_h1(device));
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H1, h1_str);
    }
    {
        g_autofree gchar *h2_str = g_strdup_printf("%u", awg_device_get_h2(device));
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H2, h2_str);
    }
    {
        g_autofree gchar *h3_str = g_strdup_printf("%u", awg_device_get_h3(device));
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H3, h3_str);
    }
    {
        g_autofree gchar *h4_str = g_strdup_printf("%u", awg_device_get_h4(device));
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H4, h4_str);
    }

    guint32 mtu = awg_device_get_mtu(device);
    if (mtu > 0) {
        g_autofree gchar *mtu_str = g_strdup_printf("%u", mtu);
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_MTU, mtu_str);
    }

    value = awg_device_get_pre_up(device);
    if (value && *value) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRE_UP, value);
    }

    value = awg_device_get_post_up(device);
    if (value && *value) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_POST_UP, value);
    }

    value = awg_device_get_pre_down(device);
    if (value && *value) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRE_DOWN, value);
    }

    value = awg_device_get_post_down(device);
    if (value && *value) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_POST_DOWN, value);
    }

    peers = awg_device_get_peers_list(device);
    for (i = 0; peers != NULL; peers = peers->next, i++) {
        AWGDevicePeer *peer = AWG_DEVICE_PEER(peers->data);
        gchar *key;

        key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PUBLIC_KEY, i);
        g_autofree gchar *peer_pubkey = awg_device_peer_dup_public_key(peer);
        if (peer_pubkey) {
            nm_setting_vpn_add_data_item(s_vpn, key, peer_pubkey);
        }
        g_free(key);

        key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PRESHARED_KEY, i);
        g_autofree gchar *peer_sharedkey = awg_device_peer_dup_shared_key(peer);
        if (peer_sharedkey) {
            nm_setting_vpn_add_secret(s_vpn, key, peer_sharedkey);
        }
        g_free(key);

        key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PRESHARED_KEY_FLAGS, i);
        nm_setting_set_secret_flags(NM_SETTING(s_vpn),
                                    key,
                                    awg_device_peer_get_shared_key_flags(peer),
                                    NULL);
        g_free(key);

        key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_ENDPOINT, i);
        value = awg_device_peer_get_endpoint(peer);
        if (value) {
            nm_setting_vpn_add_data_item(s_vpn, key, value);
        }
        g_free(key);

        key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_ALLOWED_IPS, i);
        g_autofree gchar *allowed_ips = awg_device_peer_get_allowed_ips_as_string(peer);
        if (allowed_ips) {
            nm_setting_vpn_add_data_item(s_vpn, key, allowed_ips);
        }
        g_free(key);

        key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_KEEP_ALIVE, i);
        guint16 keep_alive = awg_device_peer_get_keep_alive_interval(peer);
        g_autofree gchar *keep_alive_str = g_strdup_printf("%u", keep_alive);
        nm_setting_vpn_add_data_item(s_vpn, key, keep_alive_str);
        g_free(key);

        key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_ADVANCED_SECURITY, i);
        nm_setting_vpn_add_data_item(s_vpn, key, awg_device_peer_get_advanced_security(peer) ? "on" : "off");
        g_free(key);
    }

    return TRUE;
}

AWGDevice *
awg_device_new_from_nm_connection(NMConnection *connection, GError **error)
{
    NMSettingIP4Config *s_ip4;
    NMSettingIP6Config *s_ip6;
    NMSettingVpn *s_vpn;
    AWGDevice *device;
    const char *value;
    guint i;

    g_return_val_if_fail(NM_IS_CONNECTION(connection), NULL);

    s_ip4 = (NMSettingIP4Config *)nm_connection_get_setting_ip4_config(connection);
    s_ip6 = (NMSettingIP6Config *)nm_connection_get_setting_ip6_config(connection);
    s_vpn = nm_connection_get_setting_vpn(connection);

    if (!s_vpn) {
        g_set_error_literal(error, 1, 1, "No VPN setting found in connection");
        return NULL;
    }

    device = awg_device_new();

    value = nm_setting_vpn_get_secret(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY);
    if (value && !awg_device_set_private_key(device, value)) {
        g_warning("Failed to set private key");
    }

    {
        NMSettingSecretFlags flags;
        if (nm_setting_get_secret_flags(NM_SETTING(s_vpn), NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY, &flags, NULL)) {
            awg_device_set_private_key_flags(device, flags);
        }
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PUBLIC_KEY);
    if (value && !awg_device_set_public_key(device, value)) {
        g_warning("Failed to set public key");
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_LISTEN_PORT);
    if (value) {
        awg_device_set_listen_port_from_string(device, value);
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_FW_MARK);
    if (value) {
        awg_device_set_fw_mark_from_string(device, value);
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_JC);
    if (value) {
        awg_device_set_jc_from_string(device, value);
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_JMIN);
    if (value) {
        awg_device_set_jmin_from_string(device, value);
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_JMAX);
    if (value) {
        awg_device_set_jmax_from_string(device, value);
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_S1);
    if (value) {
        awg_device_set_s1_from_string(device, value);
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_S2);
    if (value) {
        awg_device_set_s2_from_string(device, value);
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H1);
    if (value) {
        awg_device_set_h1_from_string(device, value);
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H2);
    if (value) {
        awg_device_set_h2_from_string(device, value);
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H3);
    if (value) {
        awg_device_set_h3_from_string(device, value);
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H4);
    if (value) {
        awg_device_set_h4_from_string(device, value);
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_MTU);
    if (value && *value) {
        awg_device_set_mtu_from_string(device, value);
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRE_UP);
    if (value && *value) {
        awg_device_set_pre_up(device, value);
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_POST_UP);
    if (value && *value) {
        awg_device_set_post_up(device, value);
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRE_DOWN);
    if (value && *value) {
        awg_device_set_pre_down(device, value);
    }

    value = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_POST_DOWN);
    if (value && *value) {
        awg_device_set_post_down(device, value);
    }

    if (s_ip4) {
        guint num_addr = nm_setting_ip_config_get_num_addresses(NM_SETTING_IP_CONFIG(s_ip4));
        guint j;

        for (j = 0; j < num_addr; j++) {
            NMIPAddress *nm_addr = nm_setting_ip_config_get_address(NM_SETTING_IP_CONFIG(s_ip4), j);
            if (nm_addr) {
                const char *addr_str = nm_ip_address_get_address(nm_addr);
                guint prefix = nm_ip_address_get_prefix(nm_addr);
                if (addr_str) {
                    gchar *addr_with_prefix = g_strdup_printf("%s/%u", addr_str, prefix);
                    awg_device_set_address_from_string(device, addr_with_prefix);
                    g_free(addr_with_prefix);
                }
            }
        }

        guint num_dns = nm_setting_ip_config_get_num_dns(NM_SETTING_IP_CONFIG(s_ip4));
        GString *dns_str = g_string_new(NULL);
        for (j = 0; j < num_dns; j++) {
            const char *dns = nm_setting_ip_config_get_dns(NM_SETTING_IP_CONFIG(s_ip4), j);
            if (dns) {
                if (dns_str->len > 0) {
                    g_string_append(dns_str, ", ");
                }
                g_string_append(dns_str, dns);
            }
        }
        if (dns_str->len > 0) {
            awg_device_set_dns_from_string(device, dns_str->str);
        }
        g_string_free(dns_str, TRUE);
    }

    if (s_ip6) {
        guint num_addr = nm_setting_ip_config_get_num_addresses(NM_SETTING_IP_CONFIG(s_ip6));
        guint j;

        for (j = 0; j < num_addr; j++) {
            NMIPAddress *nm_addr = nm_setting_ip_config_get_address(NM_SETTING_IP_CONFIG(s_ip6), j);
            if (nm_addr) {
                const char *addr_str = nm_ip_address_get_address(nm_addr);
                guint prefix = nm_ip_address_get_prefix(nm_addr);
                if (addr_str) {
                    gchar *addr_with_prefix = g_strdup_printf("[%s]/%u", addr_str, prefix);
                    awg_device_set_address_from_string(device, addr_with_prefix);
                    g_free(addr_with_prefix);
                }
            }
        }

        guint num_dns = nm_setting_ip_config_get_num_dns(NM_SETTING_IP_CONFIG(s_ip6));
        GString *dns_str = g_string_new(NULL);
        for (j = 0; j < num_dns; j++) {
            const char *dns = nm_setting_ip_config_get_dns(NM_SETTING_IP_CONFIG(s_ip6), j);
            if (dns) {
                if (dns_str->len > 0) {
                    g_string_append(dns_str, ", ");
                }
                g_string_append(dns_str, dns);
            }
        }
        if (dns_str->len > 0) {
            awg_device_set_dns_from_string(device, dns_str->str);
        }
        g_string_free(dns_str, TRUE);
    }

    for (i = 0; i < 16; i++) {
        gchar *key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PUBLIC_KEY, i);
        value = nm_setting_vpn_get_data_item(s_vpn, key);

        if (!value) {
            g_free(key);
            break;
        }

        AWGDevicePeer *peer = awg_device_peer_new();

        if (!awg_device_peer_set_public_key(peer, value)) {
            g_warning("Failed to set peer public key");
            g_object_unref(peer);
            g_free(key);
            continue;
        }

        g_free(key);

        key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PRESHARED_KEY, i);
        value = nm_setting_vpn_get_secret(s_vpn, key);
        if (value) {
            awg_device_peer_set_shared_key(peer, value);
        }
        g_free(key);

        key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PRESHARED_KEY_FLAGS, i);
        {
            NMSettingSecretFlags flags;
            if (nm_setting_get_secret_flags(NM_SETTING(s_vpn), key, &flags, NULL)) {
                awg_device_peer_set_shared_key_flags(peer, flags);
            }
        }
        g_free(key);

        key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_ENDPOINT, i);
        value = nm_setting_vpn_get_data_item(s_vpn, key);
        if (value) {
            awg_device_peer_set_endpoint(peer, value);
        }
        g_free(key);

        key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_ALLOWED_IPS, i);
        value = nm_setting_vpn_get_data_item(s_vpn, key);
        if (value) {
            awg_device_peer_set_allowed_ips_from_string(peer, value);
        }
        g_free(key);

        key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_KEEP_ALIVE, i);
        value = nm_setting_vpn_get_data_item(s_vpn, key);
        if (value) {
            awg_device_peer_set_keep_alive_interval_from_string(peer, value);
        }
        g_free(key);

        key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_ADVANCED_SECURITY, i);
        value = nm_setting_vpn_get_data_item(s_vpn, key);
        if (value) {
            awg_device_peer_set_advanced_security(peer, g_strcmp0(value, "on") == 0);
        }
        g_free(key);

        awg_device_add_peer(device, peer);
        g_object_unref(peer);
    }

    return device;
}
