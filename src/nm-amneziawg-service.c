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
#include <netdb.h>
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
} Configs;

typedef struct {
    char *mgt_path;
    char *connection_file;
    gchar *connection_config;
    AWGConnectionManager *conn_manager;
    gboolean ip4_method_auto;
    gboolean ip6_method_auto;
    GTask *connect_task;
    gboolean disconnecting;
    guint config_source_id;
} NMAmneziaWGPluginPrivate;

typedef struct {
    NMVpnServicePlugin *plugin;
    NMConnection *connection;
    gchar *if_name;
    gboolean ip4_method_auto;
    gboolean ip6_method_auto;
    AWGDevice *device;
    AWGConnectionManager *conn_manager;
} ConnectTaskData;

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

static gboolean
send_config(gpointer data)
{
    Configs *cfgs = data;
    NMAmneziaWGPluginPrivate *priv =
        NM_AMNEZIAWG_PLUGIN_GET_PRIVATE(cfgs->plugin);

    priv->config_source_id = 0;
    nm_vpn_service_plugin_set_config(cfgs->plugin, cfgs->config);

    if (cfgs->ip4config) {
        nm_vpn_service_plugin_set_ip4_config(cfgs->plugin, cfgs->ip4config);
    }

    if (cfgs->ip6config) {
        nm_vpn_service_plugin_set_ip6_config(cfgs->plugin, cfgs->ip6config);
    }

    return G_SOURCE_REMOVE;
}

static void
configs_free(Configs *configs)
{
    g_clear_object(&configs->plugin);
    g_clear_pointer(&configs->config, g_variant_unref);
    g_clear_pointer(&configs->ip4config, g_variant_unref);
    g_clear_pointer(&configs->ip6config, g_variant_unref);
    g_free(configs);
}

static void
cancel_pending_config(NMAmneziaWGPluginPrivate *priv)
{
    if (priv->config_source_id) {
        g_source_remove(priv->config_source_id);
        priv->config_source_id = 0;
    }
}

static gboolean
cleanup_connection_manager(AWGConnectionManager *conn_manager,
                           gboolean ip4_method_auto,
                           gboolean ip6_method_auto,
                           GError **error)
{
    GError *cleanup_error = NULL;
    gboolean success = TRUE;

    if (!conn_manager)
        return TRUE;

    if (!awg_connection_manager_manages_routes(conn_manager)) {
        if (!ip4_method_auto &&
            !awg_connection_manager_netlink_delete_routes(conn_manager, AF_INET,
                                                          &cleanup_error)) {
            _LOGW("Warning: Could not delete IPv4 routes: %s",
                  cleanup_error ? cleanup_error->message : "unknown error");
            success = FALSE;
        }
        g_clear_error(&cleanup_error);

        if (!ip6_method_auto &&
            !awg_connection_manager_netlink_delete_routes(conn_manager, AF_INET6,
                                                          &cleanup_error)) {
            _LOGW("Warning: Could not delete IPv6 routes: %s",
                  cleanup_error ? cleanup_error->message : "unknown error");
            success = FALSE;
        }
        g_clear_error(&cleanup_error);
    }

    if (!awg_connection_manager_disconnect(conn_manager, &cleanup_error)) {
        success = FALSE;
    }

    if (cleanup_error) {
        if (error && !*error)
            *error = g_steal_pointer(&cleanup_error);
        else
            g_clear_error(&cleanup_error);
    }

    return success;
}

// disconnect from the current connection
static gboolean
wg_disconnect(NMVpnServicePlugin *plugin,
              GError **error)
{
    NMAmneziaWGPluginPrivate *priv = NM_AMNEZIAWG_PLUGIN_GET_PRIVATE(plugin);

    cancel_pending_config(priv);

    /* If a connect is in progress, mark as disconnecting and let the worker finish.
     * The callback will see the flag and clean up. */
    if (priv->connect_task) {
        _LOGI("Disconnect requested while connect is in progress, will clean up after.");
        priv->disconnecting = TRUE;
        return TRUE;
    }

    if (!priv->conn_manager) {
        _LOGI("Disconnected from AmneziaWG Connection (no active manager).");
        return TRUE;
    }

    if (!cleanup_connection_manager(priv->conn_manager,
                                    priv->ip4_method_auto,
                                    priv->ip6_method_auto,
                                    error)) {
        _LOGW("Error: Could not disconnect!");
        if (error && !*error) {
            g_set_error_literal(error, NM_VPN_PLUGIN_ERROR, NM_VPN_PLUGIN_ERROR_FAILED,
                                "Could not disconnect");
        }
        g_object_unref(priv->conn_manager);
        priv->conn_manager = NULL;
        return FALSE;
    }

    g_object_unref(priv->conn_manager);
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
// create a Config, Ip4Config and Ip6Config from AWGDevice
// and create a timer that sends the configuration to the plugin (see 'send_config()' above)
static GInetAddress *
resolve_host(const char *host, int port)
{
    struct addrinfo hints, *res = NULL;
    GInetAddress *addr = NULL;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(host, NULL, &hints, &res) == 0) {
        if (res->ai_family == AF_INET && res->ai_addrlen == sizeof(struct sockaddr_in)) {
            struct sockaddr_in *sa = (struct sockaddr_in *)res->ai_addr;
            addr = g_inet_address_new_from_bytes((guint8 *)&sa->sin_addr, AF_INET);
        } else if (res->ai_family == AF_INET6 && res->ai_addrlen == sizeof(struct sockaddr_in6)) {
            struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)res->ai_addr;
            addr = g_inet_address_new_from_bytes((guint8 *)&sa6->sin6_addr, AF_INET6);
        }
        freeaddrinfo(res);
    }

    return addr;
}

static gchar *
endpoint_get_host(const gchar *endpoint)
{
    const gchar *end;

    if (!endpoint || !*endpoint)
        return NULL;

    if (endpoint[0] == '[') {
        end = strchr(endpoint, ']');
        if (!end || end == endpoint + 1 || end[1] != ':')
            return NULL;
        return g_strndup(endpoint + 1, end - endpoint - 1);
    }

    end = strrchr(endpoint, ':');
    if (!end || end == endpoint || end[1] == '\0')
        return NULL;

    return g_strndup(endpoint, end - endpoint);
}

static gboolean
set_config(NMVpnServicePlugin *plugin, AWGDevice *device, const gchar *if_name, gboolean ip4_method_auto, gboolean ip6_method_auto)
{
    GVariantBuilder builder, ip4builder, ip6builder;
    GVariant *config, *ip4config, *ip6config;
    GVariant *val;
    const GInetAddress *addr;
    const GList *dns_list;
    const GList *peers_list;
    gchar *addr_str;
    gboolean has_ip4 = FALSE;
    gboolean has_ip6 = FALSE;
    Configs *configs = g_new0(Configs, 1);

    g_variant_builder_init(&builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_init(&ip4builder, G_VARIANT_TYPE_VARDICT);
    g_variant_builder_init(&ip6builder, G_VARIANT_TYPE_VARDICT);

    peers_list = awg_device_get_peers_list(device);
    if (!peers_list) {
        g_warning("set_config: no peers in device!");
        return FALSE;
    }

    {
        AWGDevicePeer *peer = peers_list->data;
        GInetAddress *gateway_addr = NULL;
        gboolean gateway_addr_owned = FALSE;
        const gchar *endpoint = awg_device_peer_get_endpoint(peer);

        gateway_addr = awg_device_peer_get_resolved_endpoint(peer);

        if (!gateway_addr && endpoint) {
            g_autofree gchar *host = endpoint_get_host(endpoint);

            if (host) {
                gateway_addr = g_inet_address_new_from_string(host);
                if (!gateway_addr)
                    gateway_addr = resolve_host(host, 0);
                gateway_addr_owned = gateway_addr != NULL;
            }
        }

        if (gateway_addr) {
            gchar *ip_str = g_inet_address_to_string(gateway_addr);
            if (ip_str) {
                GVariant *gateway_val = g_inet_address_get_family(gateway_addr) == AF_INET
                                            ? ip4_to_gvariant(ip_str)
                                            : ip6_to_gvariant(ip_str);
                if (gateway_val)
                    g_variant_builder_add(&builder, "{sv}", NM_VPN_PLUGIN_CONFIG_EXT_GATEWAY, gateway_val);
                g_free(ip_str);
            }
            if (gateway_addr_owned)
                g_object_unref(gateway_addr);
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
                val = g_variant_new_uint32(awg_device_get_address_v4_prefix(device));
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
                val = g_variant_new_uint32(awg_device_get_address_v6_prefix(device));
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

    config = g_variant_ref_sink(g_variant_builder_end(&builder));
    ip4config = g_variant_builder_end(&ip4builder);
    ip6config = g_variant_builder_end(&ip6builder);
    if (has_ip4)
        configs->ip4config = g_variant_ref_sink(ip4config);
    else
        g_variant_unref(ip4config);
    if (has_ip6)
        configs->ip6config = g_variant_ref_sink(ip6config);
    else
        g_variant_unref(ip6config);
    configs->plugin = g_object_ref(plugin);
    configs->config = config;
    {
        NMAmneziaWGPluginPrivate *priv = NM_AMNEZIAWG_PLUGIN_GET_PRIVATE(plugin);
        priv->config_source_id = g_timeout_add_full(G_PRIORITY_DEFAULT, 0,
                                                    send_config, configs,
                                                    (GDestroyNotify)configs_free);
    }

    return TRUE;
}

/*****************************************************************************/

static void
connect_task_data_free(ConnectTaskData *data)
{
    if (!data)
        return;
    g_clear_object(&data->plugin);
    g_clear_object(&data->connection);
    g_clear_object(&data->device);
    g_clear_object(&data->conn_manager);
    g_free(data->if_name);
    g_free(data);
}

static void
connect_worker(GTask *task, gpointer source_object, gpointer task_data, GCancellable *cancellable)
{
    ConnectTaskData *data = task_data;
    GError *error = NULL;

    data->if_name = generate_interface_name_from_connection(data->connection);

    AWGDevice *device = awg_device_new_from_nm_connection(data->connection, &error);
    if (!device) {
        if (error) {
            g_task_return_error(task, error);
        } else {
            g_task_return_new_error(task, NM_VPN_PLUGIN_ERROR, NM_VPN_PLUGIN_ERROR_FAILED,
                                    "Could not create AWG device from connection");
        }
        return;
    }

    /* Pre-flight validation: a device without PrivateKey (lost vpn.secrets)
     * or peers produces a keyless config that "awg setconf"/awg-quick rejects
     * with "Configuration parsing error". Fail early with a clear message. */
    g_autofree gchar *invalid_reason = awg_device_get_invalid_reason(device);
    if (invalid_reason) {
        g_object_unref(device);
        g_task_return_new_error(task, NM_VPN_PLUGIN_ERROR, NM_VPN_PLUGIN_ERROR_FAILED,
                                "Invalid VPN configuration: %s", invalid_reason);
        return;
    }

    AWGConnectionManager *conn_manager = awg_connection_manager_auto_new(data->if_name, device);
    if (!conn_manager) {
        g_object_unref(device);
        g_task_return_new_error(task, NM_VPN_PLUGIN_ERROR, NM_VPN_PLUGIN_ERROR_FAILED,
                                "Could not create connection manager");
        return;
    }

    if (!awg_connection_manager_connect(conn_manager, cancellable, &error)) {
        g_object_unref(conn_manager);
        g_object_unref(device);
        if (error) {
            g_task_return_error(task, error);
        } else {
            g_task_return_new_error(task, NM_VPN_PLUGIN_ERROR, NM_VPN_PLUGIN_ERROR_FAILED,
                                    "Could not connect");
        }
        return;
    }

    /* If method=manual and netlink (not external), add routes in the worker thread */
    if (!awg_connection_manager_manages_routes(conn_manager)) {
        GError *route_error = NULL;

        if (!data->ip4_method_auto) {
            if (!awg_connection_manager_netlink_add_routes(conn_manager, AF_INET, &route_error)) {
                _LOGW("Warning: Could not add IPv4 routes: %s",
                      route_error ? route_error->message : "unknown error");
                g_clear_error(&route_error);
            }
        }

        if (!data->ip6_method_auto) {
            if (!awg_connection_manager_netlink_add_routes(conn_manager, AF_INET6, &route_error)) {
                _LOGW("Warning: Could not add IPv6 routes: %s",
                      route_error ? route_error->message : "unknown error");
                g_clear_error(&route_error);
            }
        }
    }

    /* Store results for the main-thread callback */
    data->device = device;
    data->conn_manager = conn_manager;

    g_task_return_boolean(task, TRUE);
}

static void
connect_task_completed(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
    (void)user_data;
    NMVpnServicePlugin *plugin = NM_VPN_SERVICE_PLUGIN(source_object);
    NMAmneziaWGPluginPrivate *priv = NM_AMNEZIAWG_PLUGIN_GET_PRIVATE(plugin);
    GTask *task = G_TASK(res);
    ConnectTaskData *data = g_task_get_task_data(task);
    gboolean disconnecting = priv->disconnecting;
    gboolean ip4_method_auto = data->ip4_method_auto;
    gboolean ip6_method_auto = data->ip6_method_auto;
    AWGConnectionManager *conn_manager = g_steal_pointer(&data->conn_manager);
    AWGDevice *device = g_steal_pointer(&data->device);
    gchar *if_name = g_steal_pointer(&data->if_name);
    GError *error = NULL;
    gboolean success = g_task_propagate_boolean(task, &error);

    /* Drop our task reference; the remaining task_data is freed by the
     * destroy notify. Everything we still need was stolen above. */
    g_clear_object(&priv->connect_task);

    /* Disconnect was requested while connect was in progress */
    if (disconnecting) {
        priv->disconnecting = FALSE;
        if (conn_manager) {
            /* Mirror wg_disconnect cleanup: remove routes we added, then tear down. */
            if (!awg_connection_manager_manages_routes(conn_manager)) {
                GError *route_error = NULL;

                if (!ip4_method_auto) {
                    awg_connection_manager_netlink_delete_routes(conn_manager, AF_INET, &route_error);
                    g_clear_error(&route_error);
                }

                if (!ip6_method_auto) {
                    awg_connection_manager_netlink_delete_routes(conn_manager, AF_INET6, &route_error);
                    g_clear_error(&route_error);
                }
            }
            awg_connection_manager_disconnect(conn_manager, NULL);
            g_object_unref(conn_manager);
        }
        g_clear_object(&device);
        g_free(if_name);
        g_clear_error(&error);
        return;
    }

    if (!success) {
        _LOGW("Error: Could not connect: %s", error ? error->message : "unknown error");
        g_clear_error(&error);
        nm_vpn_service_plugin_failure(plugin, NM_VPN_PLUGIN_FAILURE_CONNECT_FAILED);
        g_clear_object(&device);
        g_free(if_name);
        return;
    }

    /* Success: store connection manager in priv */
    priv->conn_manager = conn_manager;
    priv->ip4_method_auto = ip4_method_auto;
    priv->ip6_method_auto = ip6_method_auto;

    if (!set_config(plugin, device, if_name, ip4_method_auto, ip6_method_auto)) {
        _LOGW("Error: Could not set config!");
        GError *cleanup_error = NULL;

        cleanup_connection_manager(priv->conn_manager,
                                   ip4_method_auto,
                                   ip6_method_auto,
                                   &cleanup_error);
        if (cleanup_error) {
            _LOGW("Error while cleaning up failed connection: %s",
                  cleanup_error->message);
            g_clear_error(&cleanup_error);
        }
        g_clear_object(&priv->conn_manager);
        nm_vpn_service_plugin_failure(plugin, NM_VPN_PLUGIN_FAILURE_CONNECT_FAILED);
    }

    g_clear_object(&device);
    g_free(if_name);
}

// the common part of both Connect() and ConnectInteractively():
// launch an async GTask that performs the blocking connect in a worker thread,
// leaving the main loop free to handle D-Bus requests (e.g. NeedSecrets)
static gboolean
connect_common(NMVpnServicePlugin *plugin,
               NMConnection *connection,
               GError **error)
{
    NMAmneziaWGPluginPrivate *priv = NM_AMNEZIAWG_PLUGIN_GET_PRIVATE(plugin);
    const char *connection_name = nm_connection_get_id(connection);
    NMSettingIPConfig *s_ip4, *s_ip6;
    const char *method_ip4, *method_ip6;
    ConnectTaskData *task_data;

    /* Reject if a connect is already in progress */
    if (priv->connect_task) {
        g_set_error_literal(error, NM_VPN_PLUGIN_ERROR, NM_VPN_PLUGIN_ERROR_WRONG_STATE,
                            "Connection attempt already in progress");
        return FALSE;
    }

    _LOGI("Setting up AmneziaWG Connection ('%s')", connection_name);

    /* Determine IP configuration methods */
    s_ip4 = nm_connection_get_setting_ip4_config(connection);
    s_ip6 = nm_connection_get_setting_ip6_config(connection);

    method_ip4 = s_ip4 ? nm_setting_ip_config_get_method(s_ip4) : NM_SETTING_IP4_CONFIG_METHOD_AUTO;
    method_ip6 = s_ip6 ? nm_setting_ip_config_get_method(s_ip6) : NM_SETTING_IP6_CONFIG_METHOD_AUTO;

    _LOGI("IP methods: IPv4=%s, IPv6=%s", method_ip4, method_ip6);

    task_data = g_new0(ConnectTaskData, 1);
    task_data->plugin = g_object_ref(plugin);
    task_data->connection = g_object_ref(connection);
    task_data->ip4_method_auto = g_str_equal(method_ip4, NM_SETTING_IP4_CONFIG_METHOD_AUTO);
    task_data->ip6_method_auto = g_str_equal(method_ip6, NM_SETTING_IP6_CONFIG_METHOD_AUTO);

    priv->connect_task = g_task_new(plugin, NULL, connect_task_completed, NULL);
    g_task_set_task_data(priv->connect_task, task_data, (GDestroyNotify)connect_task_data_free);
    g_task_run_in_thread(priv->connect_task, connect_worker);

    return TRUE;
}

// non-interactive connect
static gboolean
wg_connect(NMVpnServicePlugin *plugin,
           NMConnection *connection,
           GError **error)
{
    _LOGI("Connecting to AmneziaWG: '%s'", nm_connection_get_id(connection));
    return connect_common(plugin, connection, error);
}

// interactive connect (allows for user interaction)
static gboolean
wg_connect_interactive(NMVpnServicePlugin *plugin,
                       NMConnection *connection,
                       GVariant *details,
                       GError **error)
{
    (void)details;
    _LOGI("Connecting interactively to AmneziaWG: '%s'", nm_connection_get_id(connection));
    return connect_common(plugin, connection, error);
}

static gboolean
check_peer_need_secrets(NMSettingVpn *s_vpn, guint peer_idx, const char *key_prefix)
{
    gchar *key;
    gchar *public_key;
    const char *secret;
    NMSettingSecretFlags flags = NM_SETTING_SECRET_FLAG_NONE;
    gboolean has_flags;

    public_key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PUBLIC_KEY, peer_idx);
    if (!nm_setting_vpn_get_data_item(s_vpn, public_key)) {
        g_free(public_key);
        return FALSE;
    }
    g_free(public_key);

    key = g_strdup_printf(key_prefix, peer_idx);

    secret = nm_setting_vpn_get_secret(s_vpn, key);
    has_flags = nm_setting_get_secret_flags(NM_SETTING(s_vpn), key, &flags, NULL);

    g_free(key);

    /* A peer without an explicitly configured PSK does not need a secret.
     * Secret flags are written for configured PSKs by the editor/import path,
     * so their absence distinguishes an optional, unset PSK. */
    if (!has_flags)
        return FALSE;

    if (secret && *secret) {
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

    if ((!has_flags || !secret || !*secret) &&
        !(flags & NM_SETTING_SECRET_FLAG_NOT_REQUIRED)) {
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
        /* A FALSE return without a set error makes libnm emit
         * "g_dbus_method_invocation_take_error: assertion 'error != NULL'"
         * and NM sees "Remote peer disconnected". Always set the error. */
        g_set_error(error, NM_VPN_PLUGIN_ERROR, NM_VPN_PLUGIN_ERROR_FAILED,
                    "Required secrets for '%s' are still missing", setting_name);
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
    NMAmneziaWGPlugin *plugin = NM_AMNEZIAWG_PLUGIN(object);
    NMAmneziaWGPluginPrivate *priv = NM_AMNEZIAWG_PLUGIN_GET_PRIVATE(plugin);

    cancel_pending_config(priv);

    /* A running worker keeps the task (and the plugin via the task's
     * source reference) alive until connect_task_completed() runs, which
     * sees the disconnecting flag and cleans up. */
    if (priv->connect_task) {
        priv->disconnecting = TRUE;
        g_object_unref(priv->connect_task);
        priv->connect_task = NULL;
    }

    if (priv->conn_manager) {
        GError *error = NULL;

        if (!cleanup_connection_manager(priv->conn_manager,
                                        priv->ip4_method_auto,
                                        priv->ip6_method_auto,
                                        &error)) {
            _LOGW("Error while cleaning up connection during dispose: %s",
                  error ? error->message : "unknown error");
        }
        g_clear_error(&error);
        g_clear_object(&priv->conn_manager);
    }

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
