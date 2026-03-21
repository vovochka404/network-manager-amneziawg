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

#include "awg-connection-manager-netlink.h"
#include "awg-config.h"
#include <arpa/inet.h>
#include <errno.h>
#include <glib.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <net/if.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/socket.h>

#include "amneziawg.h"

typedef struct _AWGConnectionManagerNetlinkPrivate AWGConnectionManagerNetlinkPrivate;

struct _AWGConnectionManagerNetlink {
    GObject parent_instance;
    gint dummy;
};

struct _AWGConnectionManagerNetlinkClass {
    GObjectClass parent_class;
};

struct _AWGConnectionManagerNetlinkPrivate {
    gchar *interface_name;
    AWGDevice *device;
    gboolean running;
    gint socket_fd;
};

enum {
    PROP_0,
    PROP_INTERFACE_NAME,
    PROP_DEVICE,
    N_PROPS
};

static void awg_connection_manager_netlink_interface_init(AWGConnectionManagerInterface *iface);

G_DEFINE_TYPE_WITH_CODE(AWGConnectionManagerNetlink, awg_connection_manager_netlink, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(AWG_TYPE_CONNECTION_MANAGER, awg_connection_manager_netlink_interface_init) G_ADD_PRIVATE(AWGConnectionManagerNetlink))

#define AWG_CONNECTION_MANAGER_NETLINK_ERROR g_quark_from_static_string("awg-connection-manager-netlink-error-quark")

#define AWG_CONNECTION_MANAGER_NETLINK_GET_PRIVATE(self) \
    ((AWGConnectionManagerNetlinkPrivate *)awg_connection_manager_netlink_get_instance_private(self))

static gboolean
decode_base64_key(const gchar *base64, guint8 *key)
{
    gsize out_len = 0;
    guint8 *decoded = g_base64_decode(base64, &out_len);
    if (!decoded || out_len != 32) {
        g_free(decoded);
        return FALSE;
    }
    memcpy(key, decoded, 32);
    g_free(decoded);
    return TRUE;
}

static gboolean
add_ip_address(const gchar *ifname, const gchar *ip_with_prefix, int family)
{
    struct {
        struct nlmsghdr nlh;
        struct ifaddrmsg ifa;
        char buffer[256];
    } req;
    int sock;
    struct sockaddr_nl addr;
    char *prefix_str;
    guint prefix;

    sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0)
        return FALSE;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 0;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return FALSE;
    }

    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    req.nlh.nlmsg_type = RTM_NEWADDR;
    req.nlh.nlmsg_flags = NLM_F_REQUEST | NLM_F_CREATE | NLM_F_EXCL;
    req.nlh.nlmsg_seq = 1;
    req.nlh.nlmsg_pid = getpid();

    req.ifa.ifa_family = family;
    req.ifa.ifa_prefixlen = 0;
    req.ifa.ifa_flags = IFA_F_PERMANENT;
    req.ifa.ifa_scope = RT_SCOPE_UNIVERSE;
    req.ifa.ifa_index = if_nametoindex(ifname);

    prefix_str = strchr(ip_with_prefix, '/');
    if (prefix_str) {
        *prefix_str = '\0';
        prefix = atoi(prefix_str + 1);
    } else {
        prefix = (family == AF_INET) ? 32 : 128;
    }
    req.ifa.ifa_prefixlen = prefix;

    if (family == AF_INET) {
        struct in_addr addr_ipv4;
        if (inet_pton(AF_INET, ip_with_prefix, &addr_ipv4) > 0) {
            struct rtattr *rta = (struct rtattr *)((char *)&req.ifa + NLMSG_ALIGN(sizeof(req.ifa)));
            rta->rta_type = IFA_LOCAL;
            rta->rta_len = RTA_LENGTH(sizeof(addr_ipv4));
            memcpy(RTA_DATA(rta), &addr_ipv4, sizeof(addr_ipv4));
            req.nlh.nlmsg_len += RTA_ALIGN(rta->rta_len);

            rta = (struct rtattr *)((char *)rta + RTA_ALIGN(rta->rta_len));
            rta->rta_type = IFA_ADDRESS;
            rta->rta_len = RTA_LENGTH(sizeof(addr_ipv4));
            memcpy(RTA_DATA(rta), &addr_ipv4, sizeof(addr_ipv4));
            req.nlh.nlmsg_len += RTA_ALIGN(rta->rta_len);
        } else {
            close(sock);
            if (prefix_str)
                *prefix_str = '/';
            return FALSE;
        }
    } else {
        struct in6_addr addr_ipv6;
        if (inet_pton(AF_INET6, ip_with_prefix, &addr_ipv6) > 0) {
            struct rtattr *rta = (struct rtattr *)((char *)&req.ifa + NLMSG_ALIGN(sizeof(req.ifa)));
            rta->rta_type = IFA_LOCAL;
            rta->rta_len = RTA_LENGTH(sizeof(addr_ipv6));
            memcpy(RTA_DATA(rta), &addr_ipv6, sizeof(addr_ipv6));
            req.nlh.nlmsg_len += RTA_ALIGN(rta->rta_len);

            rta = (struct rtattr *)((char *)rta + RTA_ALIGN(rta->rta_len));
            rta->rta_type = IFA_ADDRESS;
            rta->rta_len = RTA_LENGTH(sizeof(addr_ipv6));
            memcpy(RTA_DATA(rta), &addr_ipv6, sizeof(addr_ipv6));
            req.nlh.nlmsg_len += RTA_ALIGN(rta->rta_len);
        } else {
            close(sock);
            if (prefix_str)
                *prefix_str = '/';
            return FALSE;
        }
    }

    if (prefix_str)
        *prefix_str = '/';

    if (req.nlh.nlmsg_len > sizeof(req)) {
        close(sock);
        g_warning("Netlink message too large for add_ip_address");
        return FALSE;
    }

    if (send(sock, &req, req.nlh.nlmsg_len, 0) < 0) {
        close(sock);
        return FALSE;
    }

    close(sock);
    return TRUE;
}

static gboolean
set_interface_up(const gchar *ifname, gboolean up)
{
    struct {
        struct nlmsghdr nlh;
        struct ifinfomsg ifi;
        char buffer[256];
    } req;
    int sock;
    struct sockaddr_nl addr;

    sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0)
        return FALSE;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 0;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return FALSE;
    }

    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nlh.nlmsg_type = RTM_SETLINK;
    req.nlh.nlmsg_flags = NLM_F_REQUEST;
    req.nlh.nlmsg_seq = 1;
    req.nlh.nlmsg_pid = getpid();

    req.ifi.ifi_family = AF_UNSPEC;
    req.ifi.ifi_type = 0;
    req.ifi.ifi_index = if_nametoindex(ifname);
    req.ifi.ifi_flags = up ? IFF_UP : 0;
    req.ifi.ifi_change = IFF_UP;

    if (send(sock, &req, req.nlh.nlmsg_len, 0) < 0) {
        close(sock);
        return FALSE;
    }

    close(sock);
    return TRUE;
}

static gboolean
set_interface_mtu(const gchar *ifname, guint32 mtu)
{
    struct {
        struct nlmsghdr nlh;
        struct ifinfomsg ifi;
        struct rtattr rta;
        char buffer[256];
    } req;
    int sock;
    struct sockaddr_nl addr;

    if (mtu == 0) {
        return TRUE;
    }

    sock = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
    if (sock < 0)
        return FALSE;

    memset(&addr, 0, sizeof(addr));
    addr.nl_family = AF_NETLINK;
    addr.nl_pid = getpid();
    addr.nl_groups = 0;

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return FALSE;
    }

    memset(&req, 0, sizeof(req));
    req.nlh.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifinfomsg));
    req.nlh.nlmsg_type = RTM_SETLINK;
    req.nlh.nlmsg_flags = NLM_F_REQUEST;
    req.nlh.nlmsg_seq = 1;
    req.nlh.nlmsg_pid = getpid();

    req.ifi.ifi_family = AF_UNSPEC;
    req.ifi.ifi_index = if_nametoindex(ifname);
    req.ifi.ifi_flags = 0;
    req.ifi.ifi_change = 0;

    req.rta.rta_type = IFLA_MTU;
    req.rta.rta_len = RTA_LENGTH(sizeof(mtu));
    memcpy(RTA_DATA(&req.rta), &mtu, sizeof(mtu));
    req.nlh.nlmsg_len += RTA_ALIGN(req.rta.rta_len);

    if (send(sock, &req, req.nlh.nlmsg_len, 0) < 0) {
        close(sock);
        return FALSE;
    }

    close(sock);
    return TRUE;
}

static gboolean
awg_connection_manager_netlink_connect(AWGConnectionManager *mgr, GError **error)
{
    AWGConnectionManagerNetlink *self = AWG_CONNECTION_MANAGER_NETLINK(mgr);
    AWGConnectionManagerNetlinkPrivate *priv = AWG_CONNECTION_MANAGER_NETLINK_GET_PRIVATE(self);
    wg_device *dev = NULL;
    const gchar *ifname = priv->interface_name;
    const GList *peers_list;
    const GList *iter;
    gboolean iface_added = FALSE;
    gboolean success = FALSE;

    dev = calloc(1, sizeof(wg_device));
    if (!dev) {
        g_set_error(error, AWG_CONNECTION_MANAGER_NETLINK_ERROR, ENOMEM,
                    "Failed to allocate memory");
        return FALSE;
    }

    g_strlcpy(dev->name, ifname, IFNAMSIZ);

    const gchar *private_key_str = awg_device_get_private_key(priv->device);
    if (private_key_str && strlen(private_key_str) > 0) {
        if (decode_base64_key(private_key_str, dev->private_key)) {
            dev->flags |= WGDEVICE_HAS_PRIVATE_KEY;
        }
    }

    const gchar *public_key_str = awg_device_get_public_key(priv->device);
    if (public_key_str && strlen(public_key_str) > 0) {
        if (decode_base64_key(public_key_str, dev->public_key)) {
            dev->flags |= WGDEVICE_HAS_PUBLIC_KEY;
        }
    }

    guint16 listen_port = awg_device_get_listen_port(priv->device);
    if (listen_port > 0) {
        dev->listen_port = listen_port;
        dev->flags |= WGDEVICE_HAS_LISTEN_PORT;
    }

    guint32 fw_mark = awg_device_get_fw_mark(priv->device);
    if (fw_mark > 0) {
        dev->fwmark = fw_mark;
        dev->flags |= WGDEVICE_HAS_FWMARK;
    }

    guint16 jc = awg_device_get_jc(priv->device);
    if (jc > 0) {
        dev->junk_packet_count = jc;
        dev->flags |= WGDEVICE_HAS_JC;
    }
    guint16 jmin = awg_device_get_jmin(priv->device);
    if (jmin > 0) {
        dev->junk_packet_min_size = jmin;
        dev->flags |= WGDEVICE_HAS_JMIN;
    }
    guint16 jmax = awg_device_get_jmax(priv->device);
    if (jmax > 0) {
        dev->junk_packet_max_size = jmax;
        dev->flags |= WGDEVICE_HAS_JMAX;
    }
    guint16 s1 = awg_device_get_s1(priv->device);
    if (s1 > 0) {
        dev->init_packet_junk_size = s1;
        dev->flags |= WGDEVICE_HAS_S1;
    }
    guint16 s2 = awg_device_get_s2(priv->device);
    if (s2 > 0) {
        dev->response_packet_junk_size = s2;
        dev->flags |= WGDEVICE_HAS_S2;
    }
    guint16 s3 = awg_device_get_s3(priv->device);
    if (s3 > 0) {
        dev->cookie_reply_packet_junk_size = s3;
        dev->flags |= WGDEVICE_HAS_S3;
    }
    guint16 s4 = awg_device_get_s4(priv->device);
    if (s4 > 0) {
        dev->transport_packet_junk_size = s4;
        dev->flags |= WGDEVICE_HAS_S4;
    }

    const gchar *h1 = awg_device_get_h1(priv->device);
    if (h1 && h1[0]) {
        dev->init_packet_magic_header = g_strdup(h1);
        dev->flags |= WGDEVICE_HAS_H1;
    }
    const gchar *h2 = awg_device_get_h2(priv->device);
    if (h2 && h2[0]) {
        dev->response_packet_magic_header = g_strdup(h2);
        dev->flags |= WGDEVICE_HAS_H2;
    }
    const gchar *h3 = awg_device_get_h3(priv->device);
    if (h3 && h3[0]) {
        dev->underload_packet_magic_header = g_strdup(h3);
        dev->flags |= WGDEVICE_HAS_H3;
    }
    const gchar *h4 = awg_device_get_h4(priv->device);
    if (h4 && h4[0]) {
        dev->transport_packet_magic_header = g_strdup(h4);
        dev->flags |= WGDEVICE_HAS_H4;
    }
    const gchar *i1 = awg_device_get_i1(priv->device);
    if (i1 && i1[0]) {
        dev->i1 = g_strdup(i1);
        dev->flags |= WGDEVICE_HAS_I1;
    }
    const gchar *i2 = awg_device_get_i2(priv->device);
    if (i2 && i2[0]) {
        dev->i2 = g_strdup(i2);
        dev->flags |= WGDEVICE_HAS_I2;
    }
    const gchar *i3 = awg_device_get_i3(priv->device);
    if (i3 && i3[0]) {
        dev->i3 = g_strdup(i3);
        dev->flags |= WGDEVICE_HAS_I3;
    }
    const gchar *i4 = awg_device_get_i4(priv->device);
    if (i4 && i4[0]) {
        dev->i4 = g_strdup(i4);
        dev->flags |= WGDEVICE_HAS_I4;
    }
    const gchar *i5 = awg_device_get_i5(priv->device);
    if (i5 && i5[0]) {
        dev->i5 = g_strdup(i5);
        dev->flags |= WGDEVICE_HAS_I5;
    }

    dev->flags |= WGDEVICE_REPLACE_PEERS;

    if (wg_add_device(ifname) != 0) {
        g_set_error(error, AWG_CONNECTION_MANAGER_NETLINK_ERROR, errno,
                    "Failed to add device %s", ifname);
        goto cleanup;
    }
    iface_added = TRUE;

    peers_list = awg_device_get_peers_list(priv->device);
    for (iter = peers_list; iter; iter = g_list_next(iter)) {
        AWGDevicePeer *awg_peer = iter->data;
        wg_peer *new_peer = calloc(1, sizeof(wg_peer));
        const gchar *peer_public_key;
        const gchar *peer_shared_key;
        const gchar *endpoint;
        const gchar *allowed_ips;
        guint16 keep_alive;

        if (!new_peer)
            continue;

        peer_public_key = awg_device_peer_get_public_key(awg_peer);
        if (peer_public_key && decode_base64_key(peer_public_key, new_peer->public_key)) {
            new_peer->flags |= WGPEER_HAS_PUBLIC_KEY;
        }

        peer_shared_key = awg_device_peer_get_shared_key(awg_peer);
        if (peer_shared_key && strlen(peer_shared_key) > 0) {
            if (decode_base64_key(peer_shared_key, new_peer->preshared_key)) {
                new_peer->flags |= WGPEER_HAS_PRESHARED_KEY;
            }
        }

        endpoint = awg_device_peer_get_endpoint(awg_peer);
        if (endpoint) {
            char *host = g_strdup(endpoint);
            char *port = strrchr(host, ':');
            if (port) {
                *port = '\0';
                port++;
                struct addrinfo hints, *res;
                memset(&hints, 0, sizeof(hints));
                hints.ai_family = AF_UNSPEC;
                hints.ai_socktype = SOCK_DGRAM;
                if (getaddrinfo(host, port, &hints, &res) == 0) {
                    if (res->ai_family == AF_INET && res->ai_addrlen == sizeof(new_peer->endpoint.addr4)) {
                        memcpy(&new_peer->endpoint.addr4, res->ai_addr, sizeof(new_peer->endpoint.addr4));
                    } else if (res->ai_family == AF_INET6 && res->ai_addrlen == sizeof(new_peer->endpoint.addr6)) {
                        memcpy(&new_peer->endpoint.addr6, res->ai_addr, sizeof(new_peer->endpoint.addr6));
                    }
                    freeaddrinfo(res);
                }
            }
            g_free(host);
        }

        allowed_ips = awg_device_peer_get_allowed_ips_as_string(awg_peer);
        if (allowed_ips && strlen(allowed_ips) > 0) {
            char **ip_list = g_strsplit(allowed_ips, ",", -1);
            for (int i = 0; ip_list[i]; i++) {
                char *ip_str = g_strstrip(ip_list[i]);
                char *slash = strchr(ip_str, '/');
                guint cidr = 0;
                gboolean is_ipv6 = (strchr(ip_str, ':') != NULL);

                if (slash) {
                    *slash = '\0';
                    cidr = atoi(slash + 1);
                } else {
                    cidr = is_ipv6 ? 128 : 32;
                }

                wg_allowedip *allowedip = calloc(1, sizeof(wg_allowedip));
                if (allowedip) {
                    allowedip->family = is_ipv6 ? AF_INET6 : AF_INET;
                    allowedip->cidr = cidr;
                    if (is_ipv6) {
                        inet_pton(AF_INET6, ip_str, &allowedip->ip6);
                    } else {
                        inet_pton(AF_INET, ip_str, &allowedip->ip4);
                    }

                    if (!new_peer->first_allowedip) {
                        new_peer->first_allowedip = allowedip;
                        new_peer->last_allowedip = allowedip;
                    } else {
                        new_peer->last_allowedip->next_allowedip = allowedip;
                        new_peer->last_allowedip = allowedip;
                    }
                }
            }
            g_strfreev(ip_list);
        }

        keep_alive = awg_device_peer_get_keep_alive_interval(awg_peer);
        if (keep_alive > 0) {
            new_peer->persistent_keepalive_interval = keep_alive;
            new_peer->flags |= WGPEER_HAS_PERSISTENT_KEEPALIVE_INTERVAL;
        }

        if (awg_device_peer_get_advanced_security(awg_peer)) {
            new_peer->awg = true;
            new_peer->flags |= WGPEER_HAS_ADVANCED_SECURITY;
        }

        if (!dev->first_peer) {
            dev->first_peer = new_peer;
            dev->last_peer = new_peer;
        } else {
            dev->last_peer->next_peer = new_peer;
            dev->last_peer = new_peer;
        }
    }

    if (wg_set_device(dev) != 0) {
        g_set_error(error, AWG_CONNECTION_MANAGER_NETLINK_ERROR, errno,
                    "Failed to set device %s", ifname);
        goto cleanup;
    }

    {
        const GInetAddress *addr_v4 = awg_device_get_address_v4(priv->device);
        if (addr_v4) {
            g_autofree gchar *addr_str = g_inet_address_to_string((GInetAddress *)addr_v4);
            g_autofree gchar *addr_with_prefix = g_strdup_printf("%s/32", addr_str);
            add_ip_address(ifname, addr_with_prefix, AF_INET);
        }

        const GInetAddress *addr_v6 = awg_device_get_address_v6(priv->device);
        if (addr_v6) {
            g_autofree gchar *addr_str = g_inet_address_to_string((GInetAddress *)addr_v6);
            g_autofree gchar *addr_with_prefix = g_strdup_printf("%s/128", addr_str);
            add_ip_address(ifname, addr_with_prefix, AF_INET6);
        }
    }

    if (!set_interface_up(ifname, TRUE)) {
        g_set_error(error, AWG_CONNECTION_MANAGER_NETLINK_ERROR, errno,
                    "Failed to set interface %s up", ifname);
        goto cleanup;
    }

    guint32 mtu = awg_device_get_mtu(priv->device);
    if (mtu > 0) {
        if (!set_interface_mtu(ifname, mtu)) {
            g_set_error(error, AWG_CONNECTION_MANAGER_NETLINK_ERROR, errno,
                        "Failed to set interface %s mtu to %u", ifname, mtu);
            goto cleanup;
        }
    }

    priv->running = TRUE;
    success = TRUE;

cleanup:
    wg_free_device(dev);
    if (iface_added && !success)
        wg_del_device(ifname);

    return success;
}

static gboolean
awg_connection_manager_netlink_disconnect(AWGConnectionManager *mgr, GError **error)
{
    AWGConnectionManagerNetlink *self = AWG_CONNECTION_MANAGER_NETLINK(mgr);
    AWGConnectionManagerNetlinkPrivate *priv = AWG_CONNECTION_MANAGER_NETLINK_GET_PRIVATE(self);
    const gchar *ifname = priv->interface_name;
    gboolean success = FALSE;

    set_interface_up(ifname, FALSE);

    if (wg_del_device(ifname) != 0) {
        g_set_error(error, AWG_CONNECTION_MANAGER_NETLINK_ERROR, errno,
                    "Failed to delete device %s", ifname);
        return FALSE;
    }

    priv->running = FALSE;
    success = TRUE;

    return success;
}

static gboolean
awg_connection_manager_netlink_manages_routes(AWGConnectionManager *mgr)
{
    return FALSE;
}

static void
awg_connection_manager_netlink_interface_init(AWGConnectionManagerInterface *iface)
{
    iface->connect = awg_connection_manager_netlink_connect;
    iface->disconnect = awg_connection_manager_netlink_disconnect;
    iface->manages_routes = awg_connection_manager_netlink_manages_routes;
}

static void
awg_connection_manager_netlink_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    AWGConnectionManagerNetlink *self = AWG_CONNECTION_MANAGER_NETLINK(object);
    AWGConnectionManagerNetlinkPrivate *priv = AWG_CONNECTION_MANAGER_NETLINK_GET_PRIVATE(self);

    switch (prop_id) {
    case PROP_INTERFACE_NAME:
        priv->interface_name = g_value_dup_string(value);
        break;
    case PROP_DEVICE:
        if (priv->device)
            g_object_unref(priv->device);
        priv->device = g_value_dup_object(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
awg_connection_manager_netlink_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    AWGConnectionManagerNetlink *self = AWG_CONNECTION_MANAGER_NETLINK(object);
    AWGConnectionManagerNetlinkPrivate *priv = AWG_CONNECTION_MANAGER_NETLINK_GET_PRIVATE(self);

    switch (prop_id) {
    case PROP_INTERFACE_NAME:
        g_value_set_string(value, priv->interface_name);
        break;
    case PROP_DEVICE:
        g_value_set_object(value, priv->device);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
awg_connection_manager_netlink_finalize(GObject *object)
{
    AWGConnectionManagerNetlink *self = AWG_CONNECTION_MANAGER_NETLINK(object);
    AWGConnectionManagerNetlinkPrivate *priv = AWG_CONNECTION_MANAGER_NETLINK_GET_PRIVATE(self);

    g_free(priv->interface_name);
    if (priv->device)
        g_object_unref(priv->device);

    G_OBJECT_CLASS(awg_connection_manager_netlink_parent_class)->finalize(object);
}

static void
awg_connection_manager_netlink_class_init(AWGConnectionManagerNetlinkClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = awg_connection_manager_netlink_set_property;
    object_class->get_property = awg_connection_manager_netlink_get_property;
    object_class->finalize = awg_connection_manager_netlink_finalize;

    g_object_class_install_property(object_class, PROP_INTERFACE_NAME,
                                    g_param_spec_string("interface-name", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(object_class, PROP_DEVICE,
                                    g_param_spec_object("device", NULL, NULL,
                                                        AWG_TYPE_DEVICE,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
awg_connection_manager_netlink_init(AWGConnectionManagerNetlink *self)
{
}

AWGConnectionManager *
awg_connection_manager_netlink_new(const gchar *interface_name, AWGDevice *device)
{
    return g_object_new(AWG_TYPE_CONNECTION_MANAGER_NETLINK,
                        "interface-name", interface_name,
                        "device", device,
                        NULL);
}

static gboolean
load_kernel_module(void)
{
    if (g_file_test("/sys/module/amneziawg", G_FILE_TEST_EXISTS))
        return TRUE;

    GError *error = NULL;
    gchar *output = NULL;
    gchar *error_output = NULL;
    gint exit_status = 0;

    gboolean found = FALSE;

    const gchar *modprobe_paths[] = {
        "/sbin/modprobe",
        "/usr/sbin/modprobe",
        "/modprobe",
        NULL
    };

    for (int i = 0; modprobe_paths[i]; i++) {
        if (g_file_test(modprobe_paths[i], G_FILE_TEST_EXISTS)) {
            if (g_spawn_command_line_sync("modprobe amneziawg", &output, &error_output, &exit_status, &error)) {
                g_usleep(500000);
                if (g_file_test("/sys/module/amneziawg", G_FILE_TEST_EXISTS)) {
                    found = TRUE;
                }
            }
            break;
        }
    }

    if (output)
        g_free(output);
    if (error_output)
        g_free(error_output);
    if (error)
        g_error_free(error);

    return found;
}

gboolean
awg_connection_manager_netlink_is_available(void)
{
    return load_kernel_module();
}
