/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/* nm-amneziawg-service - amneziawg integration with NetworkManager
 *
 * Copyright (C) 2005 - 2008 Tim Niemueller <tim@niemueller.de>
 * Copyright (C) 2005 - 2010 Dan Williams <dcbw@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 * $Id: nm-openvpn-service.c 4232 2008-10-29 09:13:40Z tambeti $
 *
 */

#include "nm-default.h"

#include "nm-utils/nm-shared-utils.h"
#include "nm-utils/nm-vpn-plugin-macros.h"
#include <arpa/inet.h>
#include <glib-unix.h>
#include <glib.h>
#include <locale.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>

#include "awg/awg-config.h"
#include "awg/awg-connection-manager-netlink.h"
#include "awg/awg-connection-manager.h"
#include "awg/awg-device.h"
#include "awg/awg-nm-connection.h"

#if !defined(DIST_VERSION)
#define DIST_VERSION VERSION
#endif

#define NM_AWG_MAX_PEERS 16

static struct {
    gboolean debug;
    int log_level;
    GSList *pids_pending_list;
} gl = { .debug = FALSE, .log_level = LOG_INFO, .pids_pending_list = NULL };

/*****************************************************************************/

#define NM_TYPE_AMNEZIAWG_PLUGIN (nm_amneziawg_plugin_get_type())
#define NM_AMNEZIAWG_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), NM_TYPE_AMNEZIAWG_PLUGIN, NMAmneziaWGPlugin))
#define NM_AMNEZIAWG_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), NM_TYPE_AMNEZIAWG_PLUGIN, NMAmneziaWGPluginClass))
#define NM_IS_AMNEZIAWG_PLUGIN(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), NM_TYPE_AMNEZIAWG_PLUGIN))
#define NM_IS_AMNEZIAWG_PLUGIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), NM_TYPE_AMNEZIAWG_PLUGIN))
#define NM_AMNEZIAWG_PLUGIN_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), NM_TYPE_AMNEZIAWG_PLUGIN, NMAmneziaWGPluginClass))

typedef struct {
    NMVpnServicePlugin parent;
} NMAmneziaWGPlugin;

typedef struct {
    NMVpnServicePluginClass parent;
} NMAmneziaWGPluginClass;

GType nm_amneziawg_plugin_get_type(void);

NMAmneziaWGPlugin *nm_amneziawg_plugin_new(const char *bus_name);

/*****************************************************************************/

// this struct is needed for the connect-timer callback (send_configs)
typedef struct _Configs {
    NMVpnServicePlugin *plugin;
    GVariant *config;
    GVariant *ip4config;
    GVariant *ip6config;
    GVariant *dns_config;
} Configs;

typedef struct {
    gboolean interactive;
    char *mgt_path;
    char *connection_file;
    gchar *connection_config;
    AWGConnectionManager *conn_manager;
    gboolean ip4_method_auto;
    gboolean ip6_method_auto;
} NMAmneziaWGPluginPrivate;

G_DEFINE_TYPE(NMAmneziaWGPlugin, nm_amneziawg_plugin, NM_TYPE_VPN_SERVICE_PLUGIN);

#define NM_AMNEZIAWG_PLUGIN_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), NM_TYPE_AMNEZIAWG_PLUGIN, NMAmneziaWGPluginPrivate))

/*****************************************************************************/

#define _NMLOG(level, ...)                                                             \
    G_STMT_START                                                                       \
    {                                                                                  \
        if (gl.log_level >= (level)) {                                                 \
            g_print("nm-amneziawg[%ld] %-7s " _NM_UTILS_MACRO_FIRST(__VA_ARGS__) "\n", \
                    (long)getpid(),                                                    \
                    nm_utils_syslog_to_str(level)                                      \
                        _NM_UTILS_MACRO_REST(__VA_ARGS__));                            \
        }                                                                              \
    }                                                                                  \
    G_STMT_END

#define _LOGI(...) _NMLOG(LOG_NOTICE, __VA_ARGS__)
#define _LOGW(...) _NMLOG(LOG_WARNING, __VA_ARGS__)

static gchar *
generate_unique_interface_name(void)
{
    return g_strdup("awg-temp");
}

static gchar *
generate_interface_name_from_connection(NMConnection *connection)
{
    const char *uuid = nm_connection_get_uuid(connection);
    if (!uuid || !*uuid) {
        return generate_unique_interface_name();
    }

    gchar *short_uuid = g_strndup(uuid, 8);
    gchar *interface_name = g_strdup_printf("awg-%s", short_uuid);
    g_free(short_uuid);

    return interface_name;
}

/*****************************************************************************/

// disconnect from the current connection
static gboolean
wg_disconnect(NMVpnServicePlugin *plugin,
              GError **error)
{
    NMAmneziaWGPluginPrivate *priv = NM_AMNEZIAWG_PLUGIN_GET_PRIVATE(plugin);
    AWGConnectionManager *conn_manager = priv->conn_manager;

    if (!conn_manager) {
        _LOGW("Error: No connection manager found for Disconnect");
        g_set_error_literal(error,
                            NM_VPN_PLUGIN_ERROR,
                            NM_VPN_PLUGIN_ERROR_FAILED,
                            "No connection manager found for Disconnect");
        return FALSE;
    }

    /* If method=manual and netlink, plugin added routes itself — clean them up */
    if (!awg_connection_manager_manages_routes(conn_manager)) {
        GError *route_error = NULL;

        if (!priv->ip4_method_auto) {
            _LOGI("IPv4 manual mode: cleaning up routes via netlink");
            awg_connection_manager_netlink_delete_routes(conn_manager, AF_INET, &route_error);
            if (route_error) {
                _LOGW("Warning: Could not delete IPv4 routes: %s", route_error->message);
                g_error_free(route_error);
                route_error = NULL;
            }
        }

        if (!priv->ip6_method_auto) {
            _LOGI("IPv6 manual mode: cleaning up routes via netlink");
            awg_connection_manager_netlink_delete_routes(conn_manager, AF_INET6, &route_error);
            if (route_error) {
                _LOGW("Warning: Could not delete IPv6 routes: %s", route_error->message);
                g_error_free(route_error);
            }
        }
    }

    if (!awg_connection_manager_disconnect(conn_manager, error)) {
        _LOGW("Error: Could not disconnect!");
        g_object_unref(conn_manager);
        priv->conn_manager = NULL;
        return FALSE;
    }

    g_object_unref(conn_manager);
    priv->conn_manager = NULL;

    _LOGI("Disconnected from AmneziaWG Connection!");
    return TRUE;
}

// create a GVariant as expected by SetIp4Config() from the IP4 string
static GVariant *
ip4_to_gvariant(const char *str)
{
    gchar *addr;
    gchar **tmp, **tmp2;
    struct in_addr temp_addr;
    GVariant *res;

    /* Empty */
    if (!str || strlen(str) < 1) {
        return NULL;
    }

    // strip the port and subnet
    tmp = g_strsplit(str, "/", 0);
    tmp2 = g_strsplit(tmp[0], ":", 0);
    addr = g_strdup(tmp[0]);

    if (inet_pton(AF_INET, addr, &temp_addr) <= 0) {
        _LOGW("Invalid IPv4 address: %s", addr);
        res = NULL;
    } else {
        res = g_variant_new_uint32(temp_addr.s_addr);
    }

    g_strfreev(tmp);
    g_strfreev(tmp2);
    g_free(addr);

    return res;
}

// same as above, but for IP6
static GVariant *
ip6_to_gvariant(const char *str)
{
    struct in6_addr temp_addr;
    gchar *addr;
    gchar **tmp;
    GVariantBuilder builder;
    int i;

    /* Empty */
    if (!str || strlen(str) < 1) {
        return NULL;
    }

    // since we accept a subnet at the end, let's do away with that.
    tmp = g_strsplit(str, "/", 0);
    addr = g_strdup(tmp[0]);
    g_strfreev(tmp);

    if (inet_pton(AF_INET6, addr, &temp_addr) <= 0) {
        _LOGW("Invalid IPv6 address: %s", addr);
        return NULL;
    }

    g_variant_builder_init(&builder, G_VARIANT_TYPE("ay"));
    for (i = 0; i < sizeof(temp_addr); i++) {
        g_variant_builder_add(&builder, "y", ((guint8 *)&temp_addr)[i]);
    }

    return g_variant_builder_end(&builder);
}

// callback for the timer started after the configuration
// this is necessary because:
// * NetworkManager expects the functions SetConfig(), SetIp4Config() and SetIp6Config()
//   to be called before it considers a VPN connection to be started
// * the above functions cannot be called from within Connect() and ConnectInteractively()
//   directly, because they would get blocked until after the connection timeout has completed
//   (and thus, the connection be considered to have failed)
static gboolean
send_config(gpointer data)
{
    Configs *cfgs = data;

    nm_vpn_service_plugin_set_config(cfgs->plugin, cfgs->config);

    if (cfgs->ip4config) {
        nm_vpn_service_plugin_set_ip4_config(cfgs->plugin, cfgs->ip4config);
    }

    if (cfgs->ip6config) {
        nm_vpn_service_plugin_set_ip6_config(cfgs->plugin, cfgs->ip6config);
    }

    // if we don't return FALSE, it's gonna get called again and again and again and...
    return FALSE;
}

// create a Config, Ip4Config and Ip6Config from AWGDevice
// and create a timer that sends the configuration to the plugin (see 'send_config()' above)
static gboolean
set_config(NMVpnServicePlugin *plugin, AWGDevice *device, const gchar *if_name, gboolean ip4_method_auto, gboolean ip6_method_auto)
{
    GVariantBuilder builder, ip4builder, ip6builder;
    GVariantBuilder dns_builder;
    GVariant *config, *ip4config, *ip6config, *dns_config;
    GVariant *val;
    const GInetAddress *addr;
    const GList *dns_list;
    const GList *peers_list;
    gchar *addr_str;
    gboolean has_ip4 = FALSE;
    gboolean has_ip6 = FALSE;
    gboolean has_dns = FALSE;
    Configs *configs = g_new0(Configs, 1);

    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_init(&ip4builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_init(&ip6builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_init(&dns_builder, G_VARIANT_TYPE_VARDICT);

    peers_list = awg_device_get_peers_list(device);
    if (peers_list) {
        AWGDevicePeer *peer = peers_list->data;
        const gchar *endpoint = awg_device_peer_get_endpoint(peer);
        if (endpoint) {
            gchar **parts = g_strsplit(endpoint, ":", 2);
            if (parts[0]) {
                GVariant *gateway_val = ip4_to_gvariant(parts[0]);
                if (gateway_val) {
                    g_variant_builder_add(&builder, "{sv}", NM_VPN_PLUGIN_CONFIG_EXT_GATEWAY, gateway_val);
                } else {
                    gateway_val = ip6_to_gvariant(parts[0]);
                    if (gateway_val) {
                        g_variant_builder_add(&builder, "{sv}", NM_VPN_PLUGIN_CONFIG_EXT_GATEWAY, gateway_val);
                    }
                }
            }
            g_strfreev(parts);
        }

        const gchar *allowed_ips = awg_device_peer_get_allowed_ips_as_string(peer);
        if (allowed_ips && strlen(allowed_ips) > 0) {
            GPtrArray *routes4 = g_ptr_array_new_full(10, (GDestroyNotify)nm_ip_route_unref);
            GPtrArray *routes6 = g_ptr_array_new_full(10, (GDestroyNotify)nm_ip_route_unref);
            char **ip_list = g_strsplit(allowed_ips, ",", -1);

            for (int i = 0; ip_list[i]; i++) {
                char *ip_str = g_strstrip(ip_list[i]);
                gboolean is_ipv6 = (strchr(ip_str, ':') != NULL);

                /* Only add routes if NM is managing routes for this address family */
                if (is_ipv6 && !ip6_method_auto)
                    continue;
                if (!is_ipv6 && !ip4_method_auto)
                    continue;

                gchar **ip_parts = g_strsplit(ip_str, "/", 2);
                if (ip_parts[0] && ip_parts[1]) {
                    guint prefix = (guint)g_ascii_strtoull(ip_parts[1], NULL, 10);
                    GError *error = NULL;

                    NMIPRoute *route = nm_ip_route_new(is_ipv6 ? AF_INET6 : AF_INET,
                                                       ip_parts[0], prefix, NULL, -1, &error);
                    if (route) {
                        if (is_ipv6)
                            g_ptr_array_add(routes6, route);
                        else
                            g_ptr_array_add(routes4, route);
                    } else {
                        g_warning("Failed to create route for %s: %s", ip_str, error->message);
                        g_error_free(error);
                    }
                }
                g_strfreev(ip_parts);
            }
            g_strfreev(ip_list);

            if (routes4->len > 0) {
                GVariant *routes = nm_utils_ip4_routes_to_variant(routes4);
                g_variant_builder_add(&ip4builder, "{sv}", NM_VPN_PLUGIN_IP4_CONFIG_ROUTES, routes);
            }
            g_ptr_array_unref(routes4);

            if (routes6->len > 0) {
                GVariant *routes = nm_utils_ip6_routes_to_variant(routes6);
                g_variant_builder_add(&ip6builder, "{sv}", NM_VPN_PLUGIN_IP6_CONFIG_ROUTES, routes);
            }
            g_ptr_array_unref(routes6);
        }
    }

    addr = awg_device_get_address_v4(device);
    if (addr) {
        addr_str = g_inet_address_to_string((GInetAddress *)addr);
        if (addr_str) {
            val = ip4_to_gvariant(addr_str);
            if (val) {
                g_variant_builder_add(&ip4builder, "{sv}", NM_VPN_PLUGIN_IP4_CONFIG_ADDRESS, val);
                val = g_variant_new_uint32(32);
                g_variant_builder_add(&ip4builder, "{sv}", NM_VPN_PLUGIN_IP4_CONFIG_PREFIX, val);
                has_ip4 = TRUE;
            }
            g_free(addr_str);
        }
    }

    addr = awg_device_get_address_v6(device);
    if (addr) {
        addr_str = g_inet_address_to_string((GInetAddress *)addr);
        if (addr_str) {
            val = ip6_to_gvariant(addr_str);
            if (val) {
                g_variant_builder_add(&ip6builder, "{sv}", NM_VPN_PLUGIN_IP6_CONFIG_ADDRESS, val);
                val = g_variant_new_uint32(128);
                g_variant_builder_add(&ip6builder, "{sv}", NM_VPN_PLUGIN_IP6_CONFIG_PREFIX, val);
                has_ip6 = TRUE;
            }
            g_free(addr_str);
        }
    }

    dns_list = awg_device_get_dns_v4_list(device);
    if (dns_list) {
        for (const GList *iter = dns_list; iter; iter = g_list_next(iter)) {
            GInetAddress *dns_addr = G_INET_ADDRESS(iter->data);
            gchar *dns_str = g_inet_address_to_string(dns_addr);
            if (dns_str) {
                val = g_variant_new_string(dns_str);
                g_variant_builder_add(&ip4builder, "{sv}", NM_VPN_PLUGIN_IP4_CONFIG_DNS, val);
                g_free(dns_str);
                has_dns = TRUE;
            }
        }
    }

    dns_list = awg_device_get_dns_v6_list(device);
    if (dns_list) {
        for (const GList *iter = dns_list; iter; iter = g_list_next(iter)) {
            GInetAddress *dns_addr = G_INET_ADDRESS(iter->data);
            gchar *dns_str = g_inet_address_to_string(dns_addr);
            if (dns_str) {
                val = g_variant_new_string(dns_str);
                g_variant_builder_add(&ip6builder, "{sv}", NM_VPN_PLUGIN_IP6_CONFIG_DNS, val);
                g_free(dns_str);
                has_dns = TRUE;
            }
        }
    }

    val = g_variant_new_string(if_name);
    g_variant_builder_add(&builder, "{sv}", NM_VPN_PLUGIN_CONFIG_TUNDEV, val);
    g_variant_builder_add(&ip4builder, "{sv}", NM_VPN_PLUGIN_IP4_CONFIG_TUNDEV, val);

    if (!has_ip4 && !has_ip6) {
        g_free(configs);
        return FALSE;
    }

    if (has_ip4) {
        val = g_variant_new_boolean(TRUE);
        g_variant_builder_add(&builder, "{sv}", NM_VPN_PLUGIN_CONFIG_HAS_IP4, val);
    }

    if (has_ip6) {
        val = g_variant_new_boolean(TRUE);
        g_variant_builder_add(&builder, "{sv}", NM_VPN_PLUGIN_CONFIG_HAS_IP6, val);
    }

    config = g_variant_builder_end(&builder);
    ip4config = g_variant_builder_end(&ip4builder);
    ip6config = g_variant_builder_end(&ip6builder);
    dns_config = g_variant_builder_end(&dns_builder);

    configs->ip4config = (has_ip4) ? ip4config : NULL;
    configs->ip6config = (has_ip6) ? ip6config : NULL;
    configs->dns_config = (has_dns) ? dns_config : NULL;
    configs->plugin = plugin;
    configs->config = config;
    g_timeout_add(0, send_config, configs);

    return TRUE;
}

// the common part of both Connect() and ConnectInteractively():
// create a configuration string from the NMVpnServicePlugin and NMConnection,
// export this configuration to a temporary file (/tmp/CONNECTION-ID.conf)
// and call awg-quick on this script
// the temporary file gets deleted immediately after awg-quick has completed
//
// in order to be able to disconnect properly, the configuration string
// and filename are saved in the plugin's private data, such that the
// temporary file can be re-created in the Disconnect() method
static gboolean
wg_need_secrets(NMVpnServicePlugin *plugin,
                NMConnection *connection,
                const char **setting_name,
                GError **error);

static gboolean
connect_common(NMVpnServicePlugin *plugin,
               NMConnection *connection,
               GVariant *details,
               GError **error)
{
    NMAmneziaWGPluginPrivate *priv = NM_AMNEZIAWG_PLUGIN_GET_PRIVATE(plugin);
    const char *connection_name = nm_connection_get_id(connection);
    gchar *if_name = generate_interface_name_from_connection(connection);
    AWGDevice *device;
    AWGConnectionManager *conn_manager;
    NMSettingIPConfig *s_ip4, *s_ip6;
    const char *method_ip4, *method_ip6;
    gboolean ip4_method_auto, ip6_method_auto;

    _LOGI("Setting up AmneziaWG Connection ('%s')", connection_name);

    if (priv->interactive) {
        const char *setting_name;
        if (wg_need_secrets(plugin, connection, &setting_name, error)) {
            nm_vpn_service_plugin_secrets_required(plugin, _("Private key is required"), NULL);
            g_set_error(error, NM_VPN_PLUGIN_ERROR, NM_VPN_PLUGIN_ERROR_BAD_ARGUMENTS, _("Private key is required"));
            g_free(if_name);
            return FALSE;
        }
    }

    /* Determine IP configuration methods */
    s_ip4 = nm_connection_get_setting_ip4_config(connection);
    s_ip6 = nm_connection_get_setting_ip6_config(connection);

    method_ip4 = s_ip4 ? nm_setting_ip_config_get_method(s_ip4) : NM_SETTING_IP4_CONFIG_METHOD_AUTO;
    method_ip6 = s_ip6 ? nm_setting_ip_config_get_method(s_ip6) : NM_SETTING_IP6_CONFIG_METHOD_AUTO;

    ip4_method_auto = g_str_equal(method_ip4, NM_SETTING_IP4_CONFIG_METHOD_AUTO);
    ip6_method_auto = g_str_equal(method_ip6, NM_SETTING_IP6_CONFIG_METHOD_AUTO);

    _LOGI("IP methods: IPv4=%s, IPv6=%s", method_ip4, method_ip6);

    device = awg_device_new_from_nm_connection(connection, error);
    if (!device) {
        _LOGW("Error: Could not create AWG device from connection '%s'!", connection_name);
        g_free(if_name);
        return FALSE;
    }

    conn_manager = awg_connection_manager_auto_new(if_name, device);
    if (!conn_manager) {
        _LOGW("Error: Could not create connection manager for '%s'!", connection_name);
        g_object_unref(device);
        g_free(if_name);
        return FALSE;
    }

    if (!awg_connection_manager_connect(conn_manager, error)) {
        _LOGW("Error: Could not connect!");
        g_object_unref(conn_manager);
        g_object_unref(device);
        g_free(if_name);
        return FALSE;
    }

    priv->conn_manager = conn_manager;
    priv->ip4_method_auto = ip4_method_auto;
    priv->ip6_method_auto = ip6_method_auto;

    /* If method=manual and netlink (not external), plugin must add routes itself */
    if (!awg_connection_manager_manages_routes(conn_manager)) {
        GError *route_error = NULL;

        if (!ip4_method_auto) {
            _LOGI("IPv4 manual mode: adding routes via netlink");
            if (!awg_connection_manager_netlink_add_routes(conn_manager, AF_INET, &route_error)) {
                _LOGW("Warning: Could not add IPv4 routes: %s", route_error->message);
                g_error_free(route_error);
                route_error = NULL;
            }
        }

        if (!ip6_method_auto) {
            _LOGI("IPv6 manual mode: adding routes via netlink");
            if (!awg_connection_manager_netlink_add_routes(conn_manager, AF_INET6, &route_error)) {
                _LOGW("Warning: Could not add IPv6 routes: %s", route_error->message);
                g_error_free(route_error);
                route_error = NULL;
            }
        }
    }

    if (!set_config(plugin, device, if_name, ip4_method_auto, ip6_method_auto)) {
        _LOGW("Error: Could not set config!");
    }

    g_object_unref(device);
    g_free(if_name);

    return TRUE;
}

// non-interactive connect
// this version of connect is not allowed to ask the user for secrets, etc. interactively!
static gboolean
wg_connect(NMVpnServicePlugin *plugin,
           NMConnection *connection,
           GError **error)
{
    _LOGI("Connecting to AmneziaWG: '%s'", nm_connection_get_id(connection));
    return connect_common(plugin, connection, NULL, error);
}

// interactive connect (allows for user interaction)
// this is the function that is actually called when the user clicks the connection in the GUI
static gboolean
wg_connect_interactive(NMVpnServicePlugin *plugin,
                       NMConnection *connection,
                       GVariant *details,
                       GError **error)
{
    _LOGI("Connecting interactively to AmneziaWG: '%s'", nm_connection_get_id(connection));
    if (!connect_common(plugin, connection, details, error)) {
        return FALSE;
    }

    NM_AMNEZIAWG_PLUGIN_GET_PRIVATE(plugin)->interactive = TRUE;
    return TRUE;
}

static gboolean
check_peer_need_secrets(NMSettingVpn *s_vpn, guint peer_idx, const char *key_prefix)
{
    gchar *key;
    const char *secret;
    NMSettingSecretFlags flags = NM_SETTING_SECRET_FLAG_NONE;
    gboolean has_flags;

    key = g_strdup_printf(key_prefix, peer_idx);

    secret = nm_setting_vpn_get_secret(s_vpn, key);
    has_flags = nm_setting_get_secret_flags(NM_SETTING(s_vpn), key, &flags, NULL);

    g_free(key);

    if (!has_flags) {
        return FALSE;
    }

    if (secret) {
        return FALSE;
    }

    if (flags & NM_SETTING_SECRET_FLAG_NOT_REQUIRED) {
        return FALSE;
    }

    return TRUE;
}

static gboolean
wg_need_secrets(NMVpnServicePlugin *plugin,
                NMConnection *connection,
                const char **setting_name,
                GError **error)
{
    NMSettingVpn *s_vpn;
    NMSettingSecretFlags flags = NM_SETTING_SECRET_FLAG_NONE;
    const char *secret;
    gboolean has_flags;

    s_vpn = nm_connection_get_setting_vpn(connection);
    if (!s_vpn) {
        return FALSE;
    }

    secret = nm_setting_vpn_get_secret(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY);
    has_flags = nm_setting_get_secret_flags(NM_SETTING(s_vpn), NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY, &flags, NULL);

    if (has_flags && !secret && !(flags & NM_SETTING_SECRET_FLAG_NOT_REQUIRED)) {
        *setting_name = NM_SETTING_VPN_SETTING_NAME;
        return TRUE;
    }

    for (guint i = 0; i < NM_AWG_MAX_PEERS; i++) {
        if (check_peer_need_secrets(s_vpn, i, NM_AWG_VPN_CONFIG_PEER_PRESHARED_KEY)) {
            *setting_name = NM_SETTING_VPN_SETTING_NAME;
            return TRUE;
        }
    }

    return FALSE;
}

static gboolean
wg_new_secrets(NMVpnServicePlugin *plugin,
               NMConnection *connection,
               GError **error)
{
    const char *setting_name;

    if (wg_need_secrets(plugin, connection, &setting_name, error)) {
        return FALSE;
    }

    return TRUE;
}

static void
nm_amneziawg_plugin_init(NMAmneziaWGPlugin *plugin)
{
}

static void
dispose(GObject *object)
{
    G_OBJECT_CLASS(nm_amneziawg_plugin_parent_class)->dispose(object);
}

static void
nm_amneziawg_plugin_class_init(NMAmneziaWGPluginClass *plugin_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS(plugin_class);
    NMVpnServicePluginClass *parent_class = NM_VPN_SERVICE_PLUGIN_CLASS(plugin_class);

    g_type_class_add_private(object_class, sizeof(NMAmneziaWGPluginPrivate));

    object_class->dispose = dispose;

    /* virtual methods */
    parent_class->connect = wg_connect;
    parent_class->connect_interactive = wg_connect_interactive;
    parent_class->need_secrets = wg_need_secrets;
    parent_class->disconnect = wg_disconnect;
    parent_class->new_secrets = wg_new_secrets;
}

NMAmneziaWGPlugin *
nm_amneziawg_plugin_new(const char *bus_name)
{
    NMAmneziaWGPlugin *plugin;
    GError *error = NULL;

    // NOTE: owning this name must be allowed in a DBUS configuration file:
    // "/etc/dbus-1/system.d/nm-amneziawg-service.conf"
    // (an example conf file was copied to the root of this project)
    plugin = (NMAmneziaWGPlugin *)g_initable_new(NM_TYPE_AMNEZIAWG_PLUGIN, NULL, &error,
                                                 NM_VPN_SERVICE_PLUGIN_DBUS_SERVICE_NAME, bus_name,
                                                 NM_VPN_SERVICE_PLUGIN_DBUS_WATCH_PEER, !gl.debug,
                                                 NULL);

    if (!plugin) {
        _LOGW("Failed to initialize a plugin instance: %s", error->message);
        g_error_free(error);
    }

    return plugin;
}

static gboolean
signal_handler(gpointer user_data)
{
    g_main_loop_quit(user_data);
    return G_SOURCE_REMOVE;
}

static void
quit_mainloop(NMVpnServicePlugin *plugin, gpointer user_data)
{
    g_main_loop_quit((GMainLoop *)user_data);
}

int
main(int argc, char *argv[])
{
    NMAmneziaWGPlugin *plugin;
    gboolean persist = FALSE;
    GOptionContext *opt_ctx = NULL;
    gchar *bus_name = NM_DBUS_SERVICE_AMNEZIAWG;
    GError *error = NULL;
    GMainLoop *loop;

    GOptionEntry options[] = {
        { "persist", 0, 0, G_OPTION_ARG_NONE, &persist, N_("Don’t quit when VPN connection terminates"), NULL },
        { "debug", 0, 0, G_OPTION_ARG_NONE, &gl.debug, N_("Enable verbose debug logging (may expose passwords)"), NULL },
        { "bus-name", 0, 0, G_OPTION_ARG_STRING, &bus_name, N_("D-Bus name to use for this instance"), NULL },
        { NULL }
    };

#if !GLIB_CHECK_VERSION(2, 35, 0)
    g_type_init();
#endif

    if (getenv("AMNEZIAWG_DEBUG")) {
        gl.debug = TRUE;
    }

    /* locale will be set according to environment LC_* variables */
    setlocale(LC_ALL, "");

    bindtextdomain(GETTEXT_PACKAGE, NM_AMNEZIAWG_LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    /* Parse options */
    opt_ctx = g_option_context_new(NULL);
    g_option_context_set_translation_domain(opt_ctx, GETTEXT_PACKAGE);
    g_option_context_set_ignore_unknown_options(opt_ctx, FALSE);
    g_option_context_set_help_enabled(opt_ctx, TRUE);
    g_option_context_add_main_entries(opt_ctx, options, NULL);

    g_option_context_set_summary(opt_ctx,
                                 "nm-amneziawg-service provides integrated AmneziaWG capability to NetworkManager.");

    if (!g_option_context_parse(opt_ctx, &argc, &argv, &error)) {
        g_printerr("Error parsing the command line options: %s\n", error->message);
        g_option_context_free(opt_ctx);
        g_clear_error(&error);
        exit(1);
    }
    g_option_context_free(opt_ctx);

    gl.log_level = _nm_utils_ascii_str_to_int64(getenv("NM_VPN_LOG_LEVEL"),
                                                10, 0, LOG_DEBUG, -1);

    if (gl.log_level < 0)
        gl.log_level = gl.debug ? LOG_INFO : LOG_NOTICE;

    _LOGI("nm-amneziawg-service (version " DIST_VERSION ") starting...");

    // here, the plugin is initialised
    // (and the D-BUS thing is created: be careful that you're actually allowed to use the name!)
    plugin = nm_amneziawg_plugin_new(bus_name);
    if (!plugin) {
        exit(EXIT_FAILURE);
    }

    loop = g_main_loop_new(NULL, FALSE);

    if (!persist)
        g_signal_connect(plugin, "quit", G_CALLBACK(quit_mainloop), loop);

    signal(SIGPIPE, SIG_IGN);
    g_unix_signal_add(SIGTERM, signal_handler, loop);
    g_unix_signal_add(SIGINT, signal_handler, loop);

    // run the main loop
    g_main_loop_run(loop);

    // when the main loop has finsihed (be it through a signal or whatever)
    // the plugin gets shut down
    nm_vpn_service_plugin_shutdown(NM_VPN_SERVICE_PLUGIN(plugin));
    g_object_unref(plugin);
    g_main_loop_unref(loop);

    exit(EXIT_SUCCESS);
}
