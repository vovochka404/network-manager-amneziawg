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

#include "awg-device.h"
#include "awg-validate.h"
#include <NetworkManager.h>

typedef struct _AWGDevicePeer AWGDevicePeer;
typedef struct _AWGDevice AWGDevice;
typedef struct _AWGSubNet AWGSubNet;

typedef struct AWGDevicePeerPrivate AWGDevicePeerPrivate;
typedef struct AWGDevicePrivate AWGDevicePrivate;

typedef struct AWGSubNetClass AWGSubNetClass;
typedef struct AWGDeviceClass AWGDeviceClass;
typedef struct AWGDevicePeerClass AWGDevicePeerClass;

struct _AWGSubNet {
    GObject parent_instance;
    GInetAddress *address;
    guint32 mask;
};

struct _AWGDevice {
    GObject parent_instance;
};

struct _AWGDevicePeer {
    GObject parent_instance;
};

struct AWGDevicePrivate {
    gchar *private_key;
    gchar *private_key_base64;
    gchar *public_key;
    gchar *public_key_base64;
    NMSettingSecretFlags private_key_flags;
    GInetAddress *address_v4;
    GInetAddress *address_v6;
    guint16 listen_port;
    GList *dns_v4;
    GList *dns_v6;
    guint32 fw_mark;
    guint16 jc;
    guint16 jmin;
    guint16 jmax;
    guint16 s1;
    guint16 s2;
    guint16 s3;
    guint16 s4;
    gchar *h1;
    gchar *h2;
    gchar *h3;
    gchar *h4;
    gchar *i1;
    gchar *i2;
    gchar *i3;
    gchar *i4;
    gchar *i5;
    guint32 mtu;
    gchar *pre_up;
    gchar *post_up;
    gchar *pre_down;
    gchar *post_down;
    GList *peers;
};

struct AWGDevicePeerPrivate {
    gchar *public_key;
    gchar *public_key_base64;
    gchar *shared_key;
    gchar *shared_key_base64;
    NMSettingSecretFlags shared_key_flags;
    GList *allowed_ips; // list of AWGSubnet structs
    gchar *endpoint;
    guint16 keep_alive;
    gboolean advanced_security;
};

static void awg_subnet_init(AWGSubNet *self);
static void awg_subnet_class_init(AWGSubNetClass *klass);

static void awg_device_init(AWGDevice *self);
static void awg_device_class_init(AWGDeviceClass *klass);

static void awg_device_peer_init(AWGDevicePeer *self);
static void awg_device_peer_class_init(AWGDevicePeerClass *klass);

struct AWGSubNetClass {
    GObjectClass parent_class;
};

struct AWGDeviceClass {
    GObjectClass parent_class;
};

struct AWGDevicePeerClass {
    GObjectClass parent_class;
};

G_DEFINE_TYPE(AWGSubNet, awg_subnet, G_TYPE_OBJECT)
G_DEFINE_TYPE_WITH_PRIVATE(AWGDevice, awg_device, G_TYPE_OBJECT)
G_DEFINE_TYPE_WITH_PRIVATE(AWGDevicePeer, awg_device_peer, G_TYPE_OBJECT)

static void
awg_subnet_init(AWGSubNet *self)
{
    self->address = NULL;
    self->mask = 0;
}

static void
awg_subnet_finalize(GObject *object)
{
    AWGSubNet *self = AWG_SUBNET(object);
    g_clear_object(&self->address);

    G_OBJECT_CLASS(awg_subnet_parent_class)->finalize(object);
}

static void
awg_subnet_class_init(AWGSubNetClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = awg_subnet_finalize;
}

static void
awg_device_init(AWGDevice *self)
{
    AWGDevicePrivate *priv = awg_device_get_instance_private(self);

    priv->private_key = NULL;
    priv->private_key_base64 = NULL;
    priv->public_key = NULL;
    priv->public_key_base64 = NULL;
    priv->address_v4 = NULL;
    priv->address_v6 = NULL;
    priv->fw_mark = 0;
    priv->peers = NULL;
    priv->dns_v4 = NULL;
    priv->dns_v6 = NULL;
    priv->jc = 0;
    priv->jmin = 0;
    priv->jmax = 0;
    priv->s1 = 0;
    priv->s2 = 0;
    priv->s3 = 0;
    priv->s4 = 0;
    priv->h1 = NULL;
    priv->h2 = NULL;
    priv->h3 = NULL;
    priv->h4 = NULL;
    priv->i1 = NULL;
    priv->i2 = NULL;
    priv->i3 = NULL;
    priv->i4 = NULL;
    priv->i5 = NULL;
    priv->mtu = 0;
    priv->pre_up = NULL;
    priv->post_up = NULL;
    priv->pre_down = NULL;
    priv->post_down = NULL;
    priv->peers = NULL;
}

static void
awg_device_finalize(GObject *object)
{
    AWGDevice *self = AWG_DEVICE(object);
    AWGDevicePrivate *priv = awg_device_get_instance_private(self);

    g_clear_pointer(&priv->private_key, g_free);
    g_clear_pointer(&priv->private_key_base64, g_free);
    g_clear_pointer(&priv->public_key, g_free);
    g_clear_pointer(&priv->public_key_base64, g_free);
    g_clear_object(&priv->address_v4);
    g_clear_object(&priv->address_v6);
    g_list_free_full(priv->peers, (GDestroyNotify)g_object_unref);
    g_list_free_full(priv->dns_v4, (GDestroyNotify)g_object_unref);
    g_list_free_full(priv->dns_v6, (GDestroyNotify)g_object_unref);
    g_clear_pointer(&priv->h1, g_free);
    g_clear_pointer(&priv->h2, g_free);
    g_clear_pointer(&priv->h3, g_free);
    g_clear_pointer(&priv->h4, g_free);
    g_clear_pointer(&priv->i1, g_free);
    g_clear_pointer(&priv->i2, g_free);
    g_clear_pointer(&priv->i3, g_free);
    g_clear_pointer(&priv->i4, g_free);
    g_clear_pointer(&priv->i5, g_free);
    g_clear_pointer(&priv->pre_up, g_free);
    g_clear_pointer(&priv->post_up, g_free);
    g_clear_pointer(&priv->pre_down, g_free);
    g_clear_pointer(&priv->post_down, g_free);

    G_OBJECT_CLASS(awg_device_parent_class)->finalize(object);
}

static void
awg_device_class_init(AWGDeviceClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = awg_device_finalize;
}

static void
awg_device_peer_init(AWGDevicePeer *self)
{
    AWGDevicePeerPrivate *priv = awg_device_peer_get_instance_private(self);
    priv->public_key = NULL;
    priv->public_key_base64 = NULL;
    priv->shared_key = NULL;
    priv->shared_key_base64 = NULL;
    priv->endpoint = NULL;
    priv->allowed_ips = NULL;
    priv->keep_alive = 0;
    priv->advanced_security = FALSE;
}

static void
awg_device_peer_finalize(GObject *object)
{
    AWGDevicePeerPrivate *priv;

    AWGDevicePeer *self = AWG_DEVICE_PEER(object);
    priv = awg_device_peer_get_instance_private(self);

    g_clear_pointer(&priv->public_key, g_free);
    g_clear_pointer(&priv->public_key_base64, g_free);
    g_clear_pointer(&priv->shared_key, g_free);
    g_clear_pointer(&priv->shared_key_base64, g_free);
    g_clear_pointer(&priv->endpoint, g_free);
    g_list_free_full(priv->allowed_ips, (GDestroyNotify)g_object_unref);

    G_OBJECT_CLASS(awg_device_peer_parent_class)->finalize(object);
}

static void
awg_device_peer_class_init(AWGDevicePeerClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = awg_device_peer_finalize;
}

/**** Utils ****/

static gchar *
awg_key_from_base64(const gchar *base64_str)
{
    if (!base64_str || !*base64_str) {
        return NULL;
    }

    gsize len;
    gchar *decoded_str = (char *)g_base64_decode(base64_str, &len);

    if (!decoded_str) {
        return NULL;
    }

    if (len != AWG_KEY_SIZE) {
        g_free(decoded_str);
        return NULL;
    }

    return decoded_str;
}

static gboolean
awg_is_valid_endpoint(const gchar *endpoint)
{
    gchar **parts;
    gchar *host = NULL;
    gchar *port = NULL;
    gchar *endptr = NULL;
    gint64 port_num = 0;
    gboolean is_valid_hostname = TRUE;
    GInetAddress *inet_addr = NULL;
    const gchar *p;
    gunichar ch;

    if (!endpoint || *endpoint == '\0') {
        return FALSE;
    }

    if (endpoint[0] == '[') {
        const gchar *bracket_end = strchr(endpoint, ']');
        if (!bracket_end || bracket_end == endpoint + 1) {
            return FALSE;
        }
        if (bracket_end[1] != ':' || bracket_end[2] == '\0') {
            return FALSE;
        }
        host = g_strndup(endpoint + 1, bracket_end - endpoint - 1);
        port = g_strdup(bracket_end + 2);
    } else {
        parts = g_strsplit(endpoint, ":", -1);
        if (g_strv_length(parts) != 2) {
            g_strfreev(parts);
            return FALSE;
        }
        host = g_strdup(parts[0]);
        port = g_strdup(parts[1]);
        g_strfreev(parts);
    }

    if (!host || *host == '\0') {
        g_free(host);
        g_free(port);
        return FALSE;
    }

    endptr = port;
    port_num = g_ascii_strtoll(port, &endptr, 10);
    if (endptr == port || *endptr != '\0' ||
        port_num <= 0 || port_num > 65535) {
        g_free(host);
        g_free(port);
        return FALSE;
    }
    g_free(port);

    inet_addr = g_inet_address_new_from_string(host);
    if (inet_addr) {
        g_object_unref(inet_addr);
        g_free(host);
        return TRUE;
    }

    p = host;
    while (*p) {
        ch = g_utf8_get_char(p);

        if (g_unichar_iscntrl(ch) ||
            ch == '/' ||
            ch == '\\' ||
            ch == ':' ||
            ch == '*' ||
            ch == '?' ||
            ch == '"' ||
            ch == '<' ||
            ch == '>' ||
            ch == '|') {
            is_valid_hostname = FALSE;
            break;
        }

        p = g_utf8_next_char(p);
    }

    g_free(host);
    return is_valid_hostname;
}

/**** AWGSubnet ****/

AWGSubNet *
awg_subnet_new(GInetAddress *address, const guint32 mask)
{
    AWGSubNet *obj = g_object_new(AWG_TYPE_SUBNET, NULL);
    obj->address = g_object_ref(address);
    obj->mask = mask;
    return obj;
}

AWGSubNet *
awg_subnet_new_from_string(const char *str)
{
    guint mask = 0, max_mask = 0; // Значение маски по умолчанию
    guint64 tmp = 0;
    GInetAddress *address = NULL;
    AWGSubNet *subnet = NULL;
    char *mutable_str = NULL;
    char *mask_str = NULL;
    const char *address_start = str;
    const char *address_end = NULL;

    // Обработка IPv6 адресов в формате [address]/mask
    if (str[0] == '[') {
        const char *bracket_end = strchr(str, ']');
        if (bracket_end == NULL) {
            return NULL; // Неправильный формат
        }
        address_start = str + 1;   // Пропускаем '['
        address_end = bracket_end; // До ']'
        str = bracket_end + 1;     // После ']'
    }

    // Проверяем, есть ли маска в строке
    const char *slash_pos = strchr(str, '/');
    if (slash_pos != NULL) {
        // Если маска указана, извлекаем ее
        mask_str = g_strdup(slash_pos + 1);
        if (!g_ascii_string_to_unsigned(mask_str, 10, 0, 128, &tmp, NULL) ||
            tmp > 128) {
            g_free(mask_str);
            return NULL;
        }
        mask = tmp;
        g_free(mask_str);

        // Если IPv6, адрес уже извлечен
        if (address_end != NULL) {
            mutable_str = g_strndup(address_start, address_end - address_start);
            str = mutable_str;
        } else {
            // Обрезаем строку до символа '/'
            mutable_str = g_strdup(str);
            mutable_str[slash_pos - str] = '\0';
            str = mutable_str;
        }
    } else {
        // Если маски нет, и IPv6, извлекаем адрес
        if (address_end != NULL) {
            mutable_str = g_strndup(address_start, address_end - address_start);
            str = mutable_str;
        }
    }

    address = g_inet_address_new_from_string(str);
    if (address == NULL) {
        // Если адрес невалидный, возвращаем NULL
        if (mutable_str != NULL) {
            g_free(mutable_str);
        }
        return NULL;
    }

    // Устанавливаем маску по умолчанию в зависимости от типа адреса
    max_mask = (g_inet_address_get_family(address) == G_SOCKET_FAMILY_IPV4) ? 32 : 128;
    if (mask == 0) {
        mask = max_mask;
    }

    if (mask > max_mask) {
        // Если маска больше максимальной, возвращаем NULL
        g_object_unref(address);
        if (mutable_str != NULL) {
            g_free(mutable_str);
        }
        return NULL;
    }

    subnet = awg_subnet_new(address, mask);
    g_object_unref(address); // Уменьшаем счетчик ссылок на GInetAddress

    if (mutable_str != NULL) {
        g_free(mutable_str);
    }

    return subnet;
}

char *
awg_subnet_to_string(const AWGSubNet *subnet)
{
    g_autofree gchar *addr_str = NULL;

    if (subnet == NULL || subnet->address == NULL) {
        return g_strdup("");
    }

    addr_str = g_inet_address_to_string(subnet->address);
    return g_strdup_printf("%s/%u", addr_str, subnet->mask);
}

/**** AWGDevice ****/

AWGDevice *
awg_device_new(void)
{
    return g_object_new(AWG_TYPE_DEVICE, NULL);
}

gboolean
awg_device_set_private_key(AWGDevice *self, const gchar *primary_key)
{
    AWGDevicePrivate *priv;
    gchar *decoded_key;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    decoded_key = awg_key_from_base64(primary_key);
    if (!decoded_key) {
        g_warning("Failed to decode device primary key: %s", primary_key);
        return FALSE;
    }

    g_clear_pointer(&priv->private_key, g_free);
    g_clear_pointer(&priv->private_key_base64, g_free);
    priv->private_key = decoded_key;
    priv->private_key_base64 = g_strdup(primary_key);
    return TRUE;
}

const gchar *
awg_device_get_private_key(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);
    return priv->private_key_base64;
}

gboolean
awg_device_set_public_key(AWGDevice *self, const gchar *public_key)
{
    AWGDevicePrivate *priv;
    gchar *decoded_key;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    decoded_key = awg_key_from_base64(public_key);
    if (!decoded_key) {
        g_warning("Failed to decode device public key: %s", public_key);
        return FALSE;
    }

    g_clear_pointer(&priv->public_key, g_free);
    g_clear_pointer(&priv->public_key_base64, g_free);
    priv->public_key = decoded_key;
    priv->public_key_base64 = g_strdup(public_key);
    return TRUE;
}

const gchar *
awg_device_get_public_key(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);
    return priv->public_key_base64;
}

NMSettingSecretFlags
awg_device_get_private_key_flags(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NM_SETTING_SECRET_FLAG_NONE);
    priv = awg_device_get_instance_private(self);

    return priv->private_key_flags;
}

void
awg_device_set_private_key_flags(AWGDevice *self, NMSettingSecretFlags flags)
{
    AWGDevicePrivate *priv;

    g_return_if_fail(AWG_IS_DEVICE(self));
    priv = awg_device_get_instance_private(self);

    priv->private_key_flags = flags;
}

gboolean
awg_device_set_address_from_string(AWGDevice *self, const gchar *str)
{
    AWGDevicePrivate *priv;
    AWGSubNet *address;
    GSocketFamily family;
    gchar **addresses;
    guint i;
    gboolean result = TRUE;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    addresses = g_strsplit(str, ",", 0);
    for (i = 0; addresses[i] != NULL; i++) {
        gchar *trimmed = g_strstrip(addresses[i]);
        if (*trimmed == '\0') {
            continue;
        }

        address = awg_subnet_new_from_string(trimmed);
        if (!address) {
            g_warning("Failed to decode address: %s", trimmed);
            result = FALSE;
            continue;
        }

        family = g_inet_address_get_family(address->address);
        if (family == G_SOCKET_FAMILY_IPV4) {
            if (priv->address_v4) {
                g_warning("Multiple IPv4 addresses specified");
                g_object_unref(address);
                result = FALSE;
                continue;
            }
            g_clear_object(&priv->address_v4);
            priv->address_v4 = address->address;
            g_object_ref(priv->address_v4);
        } else if (family == G_SOCKET_FAMILY_IPV6) {
            if (priv->address_v6) {
                g_warning("Multiple IPv6 addresses specified");
                g_object_unref(address);
                result = FALSE;
                continue;
            }
            g_clear_object(&priv->address_v6);
            priv->address_v6 = address->address;
            g_object_ref(priv->address_v6);
        } else {
            g_warning("Unsupported address family: %d", family);
            g_object_unref(address);
            result = FALSE;
            continue;
        }
        g_object_unref(address);
    }

    g_strfreev(addresses);
    return result && (priv->address_v4 || priv->address_v6);
}

const GInetAddress *
awg_device_get_address_v4(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    return priv->address_v4;
}

const GInetAddress *
awg_device_get_address_v6(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    return priv->address_v6;
}

gboolean
awg_device_set_listen_port(AWGDevice *self, const guint64 port)
{
    AWGDevicePrivate *priv;

    if (port > G_MAXUINT16) {
        g_warning("Port number too large: %lu", port);
        return FALSE;
    }

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    priv->listen_port = (guint16)port;
    return TRUE;
}

gboolean
awg_device_set_listen_port_from_string(AWGDevice *self, const gchar *str)
{
    guint64 port;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    g_return_val_if_fail(str != NULL, FALSE);

    if (!g_ascii_string_to_unsigned(str, 10, 0, 65535, &port, NULL)) {
        g_warning("Invalid listen port number from string: %s", str);
        return FALSE;
    }

    return awg_device_set_listen_port(self, (guint16)port);
}

gboolean
awg_device_set_dns_from_string(AWGDevice *self, const gchar *str)
{
    AWGDevicePrivate *priv;
    gchar **ip_strings;
    GList *new_list_v4 = NULL;
    GList *new_list_v6 = NULL;
    GInetAddress *addr;
    guint i;

    if (!str || *str == '\0') {
        return FALSE;
    }

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    ip_strings = g_strsplit(str, ",", 0);

    for (i = 0; ip_strings[i] != NULL; i++) {
        gchar *trimmed_ip = g_strstrip(ip_strings[i]);
        if (*trimmed_ip) {
            addr = g_inet_address_new_from_string(trimmed_ip);
            if (addr != NULL) {
                if (g_inet_address_get_family(addr) == G_SOCKET_FAMILY_IPV4) {
                    new_list_v4 = g_list_append(new_list_v4, addr);
                } else {
                    new_list_v6 = g_list_append(new_list_v6, addr);
                }
            } else {
                g_warning("Invalid IP address: %s", trimmed_ip);
                g_list_free_full(new_list_v4, (GDestroyNotify)g_object_unref);
                g_list_free_full(new_list_v6, (GDestroyNotify)g_object_unref);
                g_strfreev(ip_strings);
                return FALSE;
            }
        }
    }

    g_list_free_full(priv->dns_v4, (GDestroyNotify)g_object_unref);
    g_list_free_full(priv->dns_v6, (GDestroyNotify)g_object_unref);
    priv->dns_v4 = new_list_v4;
    priv->dns_v6 = new_list_v6;

    g_strfreev(ip_strings);
    return TRUE;
}

const GList *
awg_device_get_dns_v4_list(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return priv->dns_v4;
}

guint
awg_device_get_dns_v4_count(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), 0);
    priv = awg_device_get_instance_private(self);

    return g_list_length(priv->dns_v4);
}

const GList *
awg_device_get_dns_v6_list(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return priv->dns_v6;
}

guint
awg_device_get_dns_v6_count(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), 0);
    priv = awg_device_get_instance_private(self);

    return g_list_length(priv->dns_v6);
}

guint16
awg_device_get_listen_port(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), 0);
    priv = awg_device_get_instance_private(self);

    return priv->listen_port;
    ;
}

guint32
awg_device_get_fw_mark(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), 0);
    priv = awg_device_get_instance_private(self);

    return priv->fw_mark;
}

gboolean
awg_device_set_fw_mark(AWGDevice *self, guint64 fw_mark)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (fw_mark > G_MAXUINT32) {
        g_warning("fw_mark value too large: %lu (max: %u)", fw_mark, G_MAXUINT32);
        return FALSE;
    }

    priv->fw_mark = (guint32)fw_mark;
    return TRUE;
}

gboolean
awg_device_set_fw_mark_from_string(AWGDevice *self, const gchar *fw_mark_str)
{
    guint64 fw_mark;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    g_return_val_if_fail(fw_mark_str != NULL, FALSE);

    if (!g_ascii_string_to_unsigned(fw_mark_str, 10, 0, UINT32_MAX, &fw_mark, NULL)) {
        g_warning("Invalid fw_mark string: %s", fw_mark_str);
        return FALSE;
    }

    return awg_device_set_fw_mark(self, (guint32)fw_mark);
}

guint16
awg_device_get_jc(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), 0);
    priv = awg_device_get_instance_private(self);

    return priv->jc;
}

gboolean
awg_device_set_jc(AWGDevice *self, guint64 jc)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (jc > AWG_JC_MAX) {
        g_warning("jc value too large: %lu (max: %d)", jc, AWG_JC_MAX);
        return FALSE;
    }

    priv->jc = (guint16)jc;
    return TRUE;
}

gboolean
awg_device_set_jc_from_string(AWGDevice *self, const gchar *jc_str)
{
    guint64 jc;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    g_return_val_if_fail(jc_str != NULL, FALSE);

    if (!g_ascii_string_to_unsigned(jc_str, 10, 0, UINT16_MAX, &jc, NULL)) {
        g_warning("Invalid jc: %s (expected: number 0-65535)", jc_str);
        return FALSE;
    }

    return awg_device_set_jc(self, (guint16)jc);
}

guint16
awg_device_get_jmin(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), 0);
    priv = awg_device_get_instance_private(self);

    return priv->jmin;
}

gboolean
awg_device_set_jmin(AWGDevice *self, guint64 jmin)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (jmin > AWG_JMIN_MAX) {
        g_warning("jmin value too large: %lu (max: %d)", jmin, AWG_JMIN_MAX);
        return FALSE;
    }

    priv->jmin = (guint16)jmin;
    return TRUE;
}

gboolean
awg_device_set_jmin_from_string(AWGDevice *self, const gchar *jmin_str)
{
    guint64 jmin;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    g_return_val_if_fail(jmin_str != NULL, FALSE);

    if (!g_ascii_string_to_unsigned(jmin_str, 10, 0, UINT16_MAX, &jmin, NULL)) {
        g_warning("Invalid jmin: %s (expected: number 0-65535)", jmin_str);
        return FALSE;
    }

    return awg_device_set_jmin(self, (guint16)jmin);
}

guint16
awg_device_get_jmax(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), 0);
    priv = awg_device_get_instance_private(self);

    return priv->jmax;
}

gboolean
awg_device_set_jmax(AWGDevice *self, guint64 jmax)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (jmax > AWG_JMAX_MAX) {
        g_warning("jmax value too large: %lu (max: %d)", jmax, AWG_JMAX_MAX);
        return FALSE;
    }

    priv->jmax = (guint16)jmax;
    return TRUE;
}

gboolean
awg_device_set_jmax_from_string(AWGDevice *self, const gchar *jmax_str)
{
    guint64 jmax;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    g_return_val_if_fail(jmax_str != NULL, FALSE);

    if (!g_ascii_string_to_unsigned(jmax_str, 10, 0, UINT16_MAX, &jmax, NULL)) {
        g_warning("Invalid jmax: %s (expected: number 0-65534)", jmax_str);
        return FALSE;
    }

    return awg_device_set_jmax(self, (guint16)jmax);
}

guint16
awg_device_get_s1(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), 0);
    priv = awg_device_get_instance_private(self);

    return priv->s1;
}

gboolean
awg_device_set_s1(AWGDevice *self, guint64 s1)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (s1 > AWG_JUNK_SIZE_MAX) {
        g_warning("s1 value too large: %lu (max: %d)", s1, AWG_JUNK_SIZE_MAX);
        return FALSE;
    }

    priv->s1 = (guint16)s1;
    return TRUE;
}

gboolean
awg_device_set_s1_from_string(AWGDevice *self, const gchar *s1_str)
{
    guint64 s1;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    g_return_val_if_fail(s1_str != NULL, FALSE);

    if (!g_ascii_string_to_unsigned(s1_str, 10, 0, UINT16_MAX, &s1, NULL)) {
        g_warning("Invalid s1: %s (expected: number 0-65535)", s1_str);
        return FALSE;
    }

    return awg_device_set_s1(self, (guint16)s1);
}

guint16
awg_device_get_s2(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), 0);
    priv = awg_device_get_instance_private(self);

    return priv->s2;
}

gboolean
awg_device_set_s2(AWGDevice *self, guint64 s2)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (s2 > AWG_JUNK_SIZE_MAX) {
        g_warning("s2 value too large: %lu (max: %d)", s2, AWG_JUNK_SIZE_MAX);
        return FALSE;
    }

    priv->s2 = (guint16)s2;
    return TRUE;
}

gboolean
awg_device_set_s2_from_string(AWGDevice *self, const gchar *s2_str)
{
    guint64 s2;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    g_return_val_if_fail(s2_str != NULL, FALSE);

    if (!g_ascii_string_to_unsigned(s2_str, 10, 0, UINT16_MAX, &s2, NULL)) {
        g_warning("Invalid s2: %s (expected: number 0-65535)", s2_str);
        return FALSE;
    }

    return awg_device_set_s2(self, (guint16)s2);
}

guint16
awg_device_get_s3(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), 0);
    priv = awg_device_get_instance_private(self);

    return priv->s3;
}

gboolean
awg_device_set_s3(AWGDevice *self, guint64 s3)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (s3 > AWG_S3_MAX) {
        g_warning("s3 value too large: %lu (max: %d)", s3, AWG_S3_MAX);
        return FALSE;
    }

    priv->s3 = (guint16)s3;
    return TRUE;
}

gboolean
awg_device_set_s3_from_string(AWGDevice *self, const gchar *s3_str)
{
    guint64 s3;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    g_return_val_if_fail(s3_str != NULL, FALSE);

    if (!g_ascii_string_to_unsigned(s3_str, 10, 0, AWG_S3_MAX, &s3, NULL)) {
        g_warning("Invalid s3: %s (expected: number 0-65479)", s3_str);
        return FALSE;
    }

    return awg_device_set_s3(self, (guint16)s3);
}

guint16
awg_device_get_s4(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), 0);
    priv = awg_device_get_instance_private(self);

    return priv->s4;
}

gboolean
awg_device_set_s4(AWGDevice *self, guint64 s4)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (s4 > AWG_S4_MAX) {
        g_warning("s4 value too large: %lu (max: %d)", s4, AWG_S4_MAX);
        return FALSE;
    }

    priv->s4 = (guint16)s4;
    return TRUE;
}

gboolean
awg_device_set_s4_from_string(AWGDevice *self, const gchar *s4_str)
{
    guint64 s4;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    g_return_val_if_fail(s4_str != NULL, FALSE);

    if (!g_ascii_string_to_unsigned(s4_str, 10, 0, AWG_S4_MAX, &s4, NULL)) {
        g_warning("Invalid s4: %s (expected: number 0-65503)", s4_str);
        return FALSE;
    }

    return awg_device_set_s4(self, (guint16)s4);
}

const gchar *
awg_device_get_h1(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return priv->h1;
}

gboolean
awg_device_set_h1(AWGDevice *self, const gchar *h1)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (!h1 || !*h1) {
        g_clear_pointer(&priv->h1, g_free);
        return TRUE;
    }

    if (!awg_validate_magic_header(h1)) {
        g_warning("Invalid h1 format: %s (expected: single number or range, e.g., 1 or 1-100)", h1);
        return FALSE;
    }

    g_clear_pointer(&priv->h1, g_free);
    priv->h1 = g_strdup(h1);
    return TRUE;
}

const gchar *
awg_device_get_h2(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return priv->h2;
}

gboolean
awg_device_set_h2(AWGDevice *self, const gchar *h2)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (!h2 || !*h2) {
        g_clear_pointer(&priv->h2, g_free);
        return TRUE;
    }

    if (!awg_validate_magic_header(h2)) {
        g_warning("Invalid h2 format: %s (expected: single number or range, e.g., 1 or 1-100)", h2);
        return FALSE;
    }

    g_clear_pointer(&priv->h2, g_free);
    priv->h2 = g_strdup(h2);
    return TRUE;
}

const gchar *
awg_device_get_h3(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return priv->h3;
}

gboolean
awg_device_set_h3(AWGDevice *self, const gchar *h3)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (!h3 || !*h3) {
        g_clear_pointer(&priv->h3, g_free);
        return TRUE;
    }

    if (!awg_validate_magic_header(h3)) {
        g_warning("Invalid h3 format: %s (expected: single number or range, e.g., 1 or 1-100)", h3);
        return FALSE;
    }

    g_clear_pointer(&priv->h3, g_free);
    priv->h3 = g_strdup(h3);
    return TRUE;
}

const gchar *
awg_device_get_h4(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return priv->h4;
}

gboolean
awg_device_set_h4(AWGDevice *self, const gchar *h4)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (!h4 || !*h4) {
        g_clear_pointer(&priv->h4, g_free);
        return TRUE;
    }

    if (!awg_validate_magic_header(h4)) {
        g_warning("Invalid h4 format: %s (expected: single number or range, e.g., 1 or 1-100)", h4);
        return FALSE;
    }

    g_clear_pointer(&priv->h4, g_free);
    priv->h4 = g_strdup(h4);
    return TRUE;
}

const gchar *
awg_device_get_i1(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return priv->i1;
}

gchar *
awg_device_dup_i1(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return g_strdup(priv->i1);
}

gboolean
awg_device_set_i1(AWGDevice *self, const gchar *i1)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (!i1 || !*i1) {
        g_clear_pointer(&priv->i1, g_free);
        return TRUE;
    }

    if (!awg_validate_i_packet(i1)) {
        g_warning("Invalid i1 format: %s (expected: tag format like r16, rc10, b0xDEADBEEF, tags separated by <)", i1);
        return FALSE;
    }

    g_clear_pointer(&priv->i1, g_free);
    priv->i1 = g_strdup(i1);
    return TRUE;
}

const gchar *
awg_device_get_i2(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return priv->i2;
}

gboolean
awg_device_set_i2(AWGDevice *self, const gchar *i2)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (!i2 || !*i2) {
        g_clear_pointer(&priv->i2, g_free);
        return TRUE;
    }

    if (!awg_validate_i_packet(i2)) {
        g_warning("Invalid i2 format: %s (expected: tag format like r16, rc10, b0xDEADBEEF, tags separated by <)", i2);
        return FALSE;
    }

    g_clear_pointer(&priv->i2, g_free);
    priv->i2 = g_strdup(i2);
    return TRUE;
}

const gchar *
awg_device_get_i3(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return priv->i3;
}

gboolean
awg_device_set_i3(AWGDevice *self, const gchar *i3)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (!i3 || !*i3) {
        g_clear_pointer(&priv->i3, g_free);
        return TRUE;
    }

    if (!awg_validate_i_packet(i3)) {
        g_warning("Invalid i3 format: %s (expected: tag format like r16, rc10, b0xDEADBEEF, tags separated by <)", i3);
        return FALSE;
    }

    g_clear_pointer(&priv->i3, g_free);
    priv->i3 = g_strdup(i3);
    return TRUE;
}

const gchar *
awg_device_get_i4(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return priv->i4;
}

gboolean
awg_device_set_i4(AWGDevice *self, const gchar *i4)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (!i4 || !*i4) {
        g_clear_pointer(&priv->i4, g_free);
        return TRUE;
    }

    if (!awg_validate_i_packet(i4)) {
        g_warning("Invalid i4 format: %s (expected: tag format like r16, rc10, b0xDEADBEEF, tags separated by <)", i4);
        return FALSE;
    }

    g_clear_pointer(&priv->i4, g_free);
    priv->i4 = g_strdup(i4);
    return TRUE;
}

const gchar *
awg_device_get_i5(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return priv->i5;
}

gboolean
awg_device_set_i5(AWGDevice *self, const gchar *i5)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (!i5 || !*i5) {
        g_clear_pointer(&priv->i5, g_free);
        return TRUE;
    }

    if (!awg_validate_i_packet(i5)) {
        g_warning("Invalid i5 format: %s (expected: tag format like r16, rc10, b0xDEADBEEF, tags separated by <)", i5);
        return FALSE;
    }

    g_clear_pointer(&priv->i5, g_free);
    priv->i5 = g_strdup(i5);
    return TRUE;
}

guint32
awg_device_get_mtu(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), 0);
    priv = awg_device_get_instance_private(self);

    return priv->mtu;
}

gboolean
awg_device_set_mtu(AWGDevice *self, guint32 mtu)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (mtu != 0 && (mtu < 68 || mtu > 1500)) {
        g_warning("MTU value %u is out of valid range (68-1500)", mtu);
        return FALSE;
    }

    priv->mtu = mtu;
    return TRUE;
}

gboolean
awg_device_set_mtu_from_string(AWGDevice *self, const gchar *str)
{
    guint64 mtu;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);

    if (!str || !*str) {
        return TRUE;
    }

    if (!g_ascii_string_to_unsigned(str, 10, 0, 1500, &mtu, NULL)) {
        g_warning("Invalid mtu: %s (expected: 0 or 68-1500)", str);
        return FALSE;
    }

    return awg_device_set_mtu(self, (guint32)mtu);
}

gboolean
awg_device_set_pre_up(AWGDevice *self, const gchar *script)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    g_clear_pointer(&priv->pre_up, g_free);
    priv->pre_up = g_strdup(script);
    return TRUE;
}

const gchar *
awg_device_get_pre_up(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return priv->pre_up;
}

gboolean
awg_device_set_post_up(AWGDevice *self, const gchar *script)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    g_clear_pointer(&priv->post_up, g_free);
    priv->post_up = g_strdup(script);
    return TRUE;
}

const gchar *
awg_device_get_post_up(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return priv->post_up;
}

gboolean
awg_device_set_pre_down(AWGDevice *self, const gchar *script)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    g_clear_pointer(&priv->pre_down, g_free);
    priv->pre_down = g_strdup(script);
    return TRUE;
}

const gchar *
awg_device_get_pre_down(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return priv->pre_down;
}

gboolean
awg_device_set_post_down(AWGDevice *self, const gchar *script)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    g_clear_pointer(&priv->post_down, g_free);
    priv->post_down = g_strdup(script);
    return TRUE;
}

const gchar *
awg_device_get_post_down(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);

    return priv->post_down;
}

gboolean
awg_device_add_peer(AWGDevice *self, AWGDevicePeer *peer)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    g_return_val_if_fail(peer != NULL, FALSE);
    priv = awg_device_get_instance_private(self);

    if (awg_device_peer_is_valid(peer)) {
        if (!g_list_find(priv->peers, peer)) {
            g_object_ref(peer);
            priv->peers = g_list_append(priv->peers, peer);

            return TRUE;
        }
    }

    return FALSE;
}

gboolean
awg_device_remove_peer(AWGDevice *self, guint index)
{
    AWGDevicePrivate *priv;
    GList *link;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    link = g_list_nth(priv->peers, index);
    if (!link) {
        return FALSE;
    }

    g_object_unref(link->data);
    priv->peers = g_list_delete_link(priv->peers, link);

    return TRUE;
}

gboolean
awg_device_replace_peer(AWGDevice *self, guint index, AWGDevicePeer *new_peer)
{
    AWGDevicePrivate *priv;
    GList *link;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    g_return_val_if_fail(AWG_IS_DEVICE_PEER(new_peer), FALSE);
    priv = awg_device_get_instance_private(self);

    link = g_list_nth(priv->peers, index);
    if (!link) {
        return FALSE;
    }

    g_object_unref(link->data);
    link->data = g_object_ref(new_peer);

    return TRUE;
}

const GList *
awg_device_get_peers_list(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), NULL);
    priv = awg_device_get_instance_private(self);
    return priv->peers;
}

guint
awg_device_get_peers_count(AWGDevice *self)
{
    AWGDevicePrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE(self), 0);
    priv = awg_device_get_instance_private(self);
    return g_list_length(priv->peers);
}

gboolean
awg_device_is_valid(AWGDevice *self)
{
    AWGDevicePrivate *priv;
    AWGDevicePeer *peer = NULL;
    GList *node = NULL;

    g_return_val_if_fail(AWG_IS_DEVICE(self), FALSE);
    priv = awg_device_get_instance_private(self);

    if (!priv->private_key) {
        return FALSE;
    }

    if (g_list_length(priv->peers) == 0) {
        return FALSE;
    }

    if (!awg_validate_magic_headers_no_overlap(priv->h1, priv->h2, priv->h3, priv->h4)) {
        return FALSE;
    }

    if (!awg_validate_jmin_jmax(priv->jmin, priv->jmax)) {
        return FALSE;
    }

    node = priv->peers;
    while (node) {
        peer = (AWGDevicePeer *)node->data;
        if (!awg_device_peer_is_valid(peer)) {
            return FALSE;
        }

        node = node->next;
    }

    return TRUE;
}

/**** AWGDevicePeer ****/

AWGDevicePeer *
awg_device_peer_new(void)
{
    return g_object_new(AWG_TYPE_DEVICE_PEER, NULL);
}

AWGDevicePeer *
awg_device_peer_new_clone(AWGDevicePeer *peer)
{
    AWGDevicePeerPrivate *priv;
    AWGDevicePeer *clone;
    AWGDevicePeerPrivate *clone_priv;
    const GList *l;

    g_return_val_if_fail(AWG_IS_DEVICE_PEER(peer), NULL);

    clone = awg_device_peer_new();
    priv = awg_device_peer_get_instance_private(peer);
    clone_priv = awg_device_peer_get_instance_private(clone);

    if (priv->public_key_base64)
        awg_device_peer_set_public_key(clone, priv->public_key_base64);

    if (priv->shared_key_base64)
        awg_device_peer_set_shared_key(clone, priv->shared_key_base64);

    clone_priv->shared_key_flags = priv->shared_key_flags;

    for (l = priv->allowed_ips; l; l = l->next) {
        AWGSubNet *subnet = l->data;
        gchar *subnet_str = awg_subnet_to_string(subnet);
        if (subnet_str) {
            awg_device_peer_set_allowed_ips_from_string(clone, subnet_str);
            g_free(subnet_str);
        }
    }

    if (priv->endpoint)
        awg_device_peer_set_endpoint(clone, priv->endpoint);

    awg_device_peer_set_keep_alive_interval(clone, priv->keep_alive);
    awg_device_peer_set_advanced_security(clone, priv->advanced_security);

    return clone;
}

gboolean
awg_device_peer_set_public_key(AWGDevicePeer *self,
                               const gchar *public_key)
{
    AWGDevicePeerPrivate *priv;
    gchar *decoded_key;

    g_return_val_if_fail(AWG_IS_DEVICE_PEER(self), FALSE);
    priv = awg_device_peer_get_instance_private(self);

    decoded_key = awg_key_from_base64(public_key);
    if (!decoded_key) {
        g_warning("Failed to decode peer public key: %s", public_key);
        return FALSE;
    }

    g_clear_pointer(&priv->public_key, g_free);
    g_clear_pointer(&priv->public_key_base64, g_free);
    priv->public_key = decoded_key;
    priv->public_key_base64 = g_strdup(public_key);
    return TRUE;
}

const gchar *
awg_device_peer_get_public_key(AWGDevicePeer *self)
{
    AWGDevicePeerPrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE_PEER(self), NULL);
    priv = awg_device_peer_get_instance_private(self);
    return priv->public_key_base64;
}

gboolean
awg_device_peer_set_shared_key(AWGDevicePeer *self,
                               const gchar *shared_key)
{
    AWGDevicePeerPrivate *priv;
    gchar *decoded_key;

    g_return_val_if_fail(AWG_IS_DEVICE_PEER(self), FALSE);
    priv = awg_device_peer_get_instance_private(self);

    if (!shared_key || !*shared_key) {
        g_clear_pointer(&priv->shared_key, g_free);
        g_clear_pointer(&priv->shared_key_base64, g_free);
        return TRUE;
    }

    decoded_key = awg_key_from_base64(shared_key);
    if (!decoded_key) {
        g_warning("Failed to decode peer shared key: %s", shared_key);
        return FALSE;
    }

    g_clear_pointer(&priv->shared_key, g_free);
    g_clear_pointer(&priv->shared_key_base64, g_free);
    priv->shared_key = decoded_key;
    priv->shared_key_base64 = g_strdup(shared_key);
    return TRUE;
}

const gchar *
awg_device_peer_get_shared_key(AWGDevicePeer *self)
{
    AWGDevicePeerPrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE_PEER(self), NULL);
    priv = awg_device_peer_get_instance_private(self);
    return priv->shared_key_base64;
}

NMSettingSecretFlags
awg_device_peer_get_shared_key_flags(AWGDevicePeer *self)
{
    AWGDevicePeerPrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE_PEER(self), NM_SETTING_SECRET_FLAG_NONE);
    priv = awg_device_peer_get_instance_private(self);

    return priv->shared_key_flags;
}

void
awg_device_peer_set_shared_key_flags(AWGDevicePeer *self, NMSettingSecretFlags flags)
{
    AWGDevicePeerPrivate *priv;

    g_return_if_fail(AWG_IS_DEVICE_PEER(self));
    priv = awg_device_peer_get_instance_private(self);

    priv->shared_key_flags = flags;
}

gboolean
awg_device_peer_set_allowed_ips_from_string(AWGDevicePeer *self,
                                            const gchar *allowed_ips_string)
{
    AWGDevicePeerPrivate *priv;
    gchar **ip_strings;
    GList *new_list = NULL;
    guint i;

    if (!allowed_ips_string || *allowed_ips_string == '\0') {
        return FALSE;
    }

    g_return_val_if_fail(AWG_IS_DEVICE_PEER(self), FALSE);
    priv = awg_device_peer_get_instance_private(self);

    // Разделяем строку на массив подстрок по запятой
    ip_strings = g_strsplit(allowed_ips_string, ",", 0);

    for (i = 0; ip_strings[i] != NULL; i++) {
        gchar *trimmed_ip = g_strstrip(ip_strings[i]);
        if (*trimmed_ip) { // Проверяем, что строка не пустая
            AWGSubNet *subnet = awg_subnet_new_from_string(trimmed_ip);
            if (subnet != NULL) {
                new_list = g_list_append(new_list, subnet);
            } else {
                g_warning("Invalid IP address: %s", trimmed_ip);
                g_list_free_full(new_list, (GDestroyNotify)g_object_unref);
                g_strfreev(ip_strings);
                return FALSE;
            }
        }
    }

    // Очищаем текущий список разрешенных IP-адресов
    g_list_free_full(priv->allowed_ips, (GDestroyNotify)g_object_unref);
    priv->allowed_ips = new_list;
    // Освобождаем массив подстрок
    g_strfreev(ip_strings);
    return TRUE;
}

gchar *
awg_device_peer_get_allowed_ips_as_string(AWGDevicePeer *self)
{
    AWGDevicePeerPrivate *priv;
    GString *str;
    gchar *tmp;
    GList *l;

    g_return_val_if_fail(AWG_IS_DEVICE_PEER(self), NULL);
    priv = awg_device_peer_get_instance_private(self);

    str = g_string_new(NULL);

    // Проходим по списку разрешенных IP-адресов
    for (l = priv->allowed_ips; l != NULL; l = l->next) {
        AWGSubNet *subnet = (AWGSubNet *)l->data;
        if (str->len > 0) {
            g_string_append_len(str, ", ", 2);
        }
        tmp = awg_subnet_to_string(subnet);
        g_string_append_printf(str, "%s", tmp);
        g_free(tmp);
    }

    return g_string_free(str, FALSE);
}

gboolean
awg_device_peer_set_keep_alive_interval(AWGDevicePeer *self, guint64 interval)
{
    AWGDevicePeerPrivate *priv;
    g_return_val_if_fail(AWG_IS_DEVICE_PEER(self), FALSE);

    priv = awg_device_peer_get_instance_private(self);

    if (interval < G_MAXUINT16) {
        priv->keep_alive = (guint16)interval;
        return TRUE;
    }

    return FALSE;
}

gboolean
awg_device_peer_set_keep_alive_interval_from_string(AWGDevicePeer *self, const gchar *str)
{
    guint64 interval;

    g_return_val_if_fail(AWG_IS_DEVICE_PEER(self), FALSE);
    g_return_val_if_fail(str != NULL, FALSE);

    if (g_strcmp0(str, "off") == 0) {
        return awg_device_peer_set_keep_alive_interval(self, 0);
    }

    if (!g_ascii_string_to_unsigned(str, 10, 0, UINT16_MAX, &interval, NULL)) {
        g_warning("Invalid keep alive interval: %s", str);
        return FALSE;
    }

    return awg_device_peer_set_keep_alive_interval(self, (guint16)interval);
}

guint16
awg_device_peer_get_keep_alive_interval(AWGDevicePeer *self)
{
    AWGDevicePeerPrivate *priv;
    g_return_val_if_fail(AWG_IS_DEVICE_PEER(self), 0);

    priv = awg_device_peer_get_instance_private(self);
    return priv->keep_alive;
}

gboolean
awg_device_peer_set_endpoint(AWGDevicePeer *self, const gchar *endpoint)
{
    AWGDevicePeerPrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE_PEER(self), FALSE);
    priv = awg_device_peer_get_instance_private(self);

    if (!awg_is_valid_endpoint(endpoint)) {
        return FALSE;
    }

    g_clear_pointer(&priv->endpoint, g_free);
    priv->endpoint = g_strdup(endpoint);
    return TRUE;
}

const gchar *
awg_device_peer_get_endpoint(AWGDevicePeer *self)
{
    AWGDevicePeerPrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE_PEER(self), NULL);
    priv = awg_device_peer_get_instance_private(self);

    return priv->endpoint;
}

void
awg_device_peer_set_advanced_security(AWGDevicePeer *self, gboolean enabled)
{
    AWGDevicePeerPrivate *priv;

    g_return_if_fail(AWG_IS_DEVICE_PEER(self));
    priv = awg_device_peer_get_instance_private(self);

    priv->advanced_security = enabled;
}

gboolean
awg_device_peer_get_advanced_security(AWGDevicePeer *self)
{
    AWGDevicePeerPrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE_PEER(self), FALSE);
    priv = awg_device_peer_get_instance_private(self);

    return priv->advanced_security;
}

gboolean
awg_device_peer_is_valid(AWGDevicePeer *self)
{
    AWGDevicePeerPrivate *priv;

    g_return_val_if_fail(AWG_IS_DEVICE_PEER(self), FALSE);
    priv = awg_device_peer_get_instance_private(self);

    return priv->public_key && priv->endpoint && g_list_length(priv->allowed_ips) > 0;
}

void
awg_device_peer_free(AWGDevicePeer *peer)
{
    if (peer) {
        AWGDevicePeerPrivate *priv = awg_device_peer_get_instance_private(peer);
        g_clear_pointer(&priv->public_key, g_free);
        g_clear_pointer(&priv->shared_key, g_free);
        g_list_free_full(priv->allowed_ips, (GDestroyNotify)g_object_unref);
        g_clear_object(&priv->endpoint);
        g_free(priv);
        g_object_unref(peer);
    }
}
