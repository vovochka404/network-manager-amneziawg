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

#include "awg-validate.h"

#include <arpa/inet.h>
#include <string.h>

gboolean
awg_validate_empty(const gchar *str)
{
    gchar *trimmed;

    if (!str || !*str)
        return TRUE;

    trimmed = g_strstrip(g_strdup(str));
    gboolean empty = (*trimmed == '\0');
    g_free(trimmed);

    return empty;
}

gboolean
awg_validate_ip4(const gchar *addr)
{
    if (!addr)
        return FALSE;

    struct in_addr a;
    if (inet_pton(AF_INET, addr, &a) == 1)
        return TRUE;

    gchar **parts = g_strsplit(addr, ".", -1);
    if (!parts)
        return FALSE;

    gint len = 0;
    while (parts[len])
        len++;

    if (len != 4) {
        g_strfreev(parts);
        return FALSE;
    }

    for (gint i = 0; i < 4; i++) {
        if (!g_ascii_string_to_unsigned(parts[i], 10, 0, 255, NULL, NULL)) {
            g_strfreev(parts);
            return FALSE;
        }
    }

    g_strfreev(parts);
    return TRUE;
}

gboolean
awg_validate_ip6(const gchar *addr)
{
    if (!addr)
        return FALSE;

    struct in6_addr a;
    if (inet_pton(AF_INET6, addr, &a) == 1)
        return TRUE;

    return FALSE;
}

gboolean
awg_validate_ip_or_empty(const gchar *addr)
{
    if (awg_validate_empty(addr))
        return TRUE;
    return awg_validate_ip4(addr) || awg_validate_ip6(addr);
}

gboolean
awg_validate_dns(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;

    gchar **entries = g_strsplit(str, ",", -1);
    if (!entries)
        return FALSE;

    gint len = 0;
    while (entries[len])
        len++;

    if (len == 0) {
        g_strfreev(entries);
        return FALSE;
    }

    for (gint i = 0; i < len; i++) {
        gchar *trimmed = g_strstrip(entries[i]);
        if (!*trimmed)
            continue;
        if (!awg_validate_ip4(trimmed) && !awg_validate_ip6(trimmed)) {
            g_strfreev(entries);
            return FALSE;
        }
    }

    g_strfreev(entries);
    return TRUE;
}

gboolean
awg_validate_port(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;
    return g_ascii_string_to_unsigned(str, 10, 0, 65535, NULL, NULL);
}

gboolean
awg_validate_mtu(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;
    guint64 value;
    if (!g_ascii_string_to_unsigned(str, 10, 0, 1500, &value, NULL))
        return FALSE;
    return (value == 0 || (value >= 68 && value <= 1500));
}

gboolean
awg_validate_jc(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;
    return g_ascii_string_to_unsigned(str, 10, AWG_JC_MIN, AWG_JC_MAX, NULL, NULL);
}

gboolean
awg_validate_junk_size(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;
    return g_ascii_string_to_unsigned(str, 10, AWG_JUNK_SIZE_MIN, AWG_JUNK_SIZE_MAX, NULL, NULL);
}

gboolean
awg_validate_header_size(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;
    return g_ascii_string_to_unsigned(str, 10, AWG_HEADER_SIZE_MIN, AWG_HEADER_SIZE_MAX, NULL, NULL);
}

gboolean
awg_validate_keep_alive(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;
    return g_ascii_string_to_unsigned(str, 10, 0, 450, NULL, NULL);
}

gboolean
awg_validate_base64(const gchar *str)
{
    if (!str)
        return FALSE;

    const gchar *ptr = str;
    gint padding = 0;

    for (; ptr && *ptr; ptr++) {
        if (*ptr == '=') {
            padding++;
        } else if (padding <= 0) {
            if (!g_ascii_isalnum(*ptr) && *ptr != '+' && *ptr != '/') {
                return FALSE;
            }
        } else {
            if (*ptr != '=') {
                return FALSE;
            }
        }
    }

    if (padding > 3)
        return FALSE;

    return TRUE;
}

gboolean
awg_validate_fqdn(const gchar *str)
{
    if (!str)
        return FALSE;

    if (awg_validate_ip4(str) || awg_validate_ip6(str))
        return FALSE;

    gint dots = 0;
    for (const gchar *p = str; *p; p++) {
        if (*p == '.')
            dots++;
    }

    if (dots == 0)
        return FALSE;

    gchar **parts = g_strsplit_set(str, ".", -1);
    if (!parts)
        return FALSE;

    gint len = 0;
    while (parts[len])
        len++;

    gboolean contains_alpha = FALSE;

    for (gint i = 0; i < len; i++) {
        if (awg_validate_empty(parts[i])) {
            g_strfreev(parts);
            return FALSE;
        }

        for (const gchar *p = parts[i]; *p; p++) {
            if (*p == ':' && i == len - 1)
                break;

            if (!g_ascii_isalnum(*p) && *p != '-') {
                g_strfreev(parts);
                return FALSE;
            }

            if (g_ascii_isalpha(*p))
                contains_alpha = TRUE;
        }
    }

    g_strfreev(parts);

    if (!contains_alpha)
        return FALSE;

    return TRUE;
}

gboolean
awg_validate_subnet(const gchar *str)
{
    if (!str)
        return FALSE;

    gchar **parts = g_strsplit(str, "/", 2);
    if (!parts || !parts[0] || !parts[1]) {
        g_strfreev(parts);
        return FALSE;
    }

    gboolean is_ipv4 = awg_validate_ip4(parts[0]);
    gboolean is_ipv6 = awg_validate_ip6(parts[0]);

    if (!is_ipv4 && !is_ipv6) {
        g_strfreev(parts);
        return FALSE;
    }

    guint64 prefix;
    guint max_prefix = is_ipv4 ? 32 : 128;
    gboolean valid_prefix = g_ascii_string_to_unsigned(parts[1], 10, 0, max_prefix, &prefix, NULL);

    g_strfreev(parts);

    return valid_prefix;
}

gboolean
awg_validate_allowed_ips(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;

    gchar **entries = g_strsplit(str, ",", -1);
    if (!entries)
        return FALSE;

    gint len = 0;
    while (entries[len])
        len++;

    if (len == 0) {
        g_strfreev(entries);
        return FALSE;
    }

    for (gint i = 0; i < len; i++) {
        gchar *trimmed = g_strstrip(entries[i]);
        if (!*trimmed)
            continue;
        if (!awg_validate_subnet(trimmed)) {
            g_strfreev(entries);
            return FALSE;
        }
    }

    g_strfreev(entries);
    return TRUE;
}

gboolean
awg_validate_endpoint(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;

    gchar *host = NULL;
    gchar *port_str = NULL;
    gboolean has_colon = (g_strrstr(str, ":") != NULL);

    if (str[0] == '[') {
        const gchar *close_bracket = g_strrstr(str, "]");
        if (!close_bracket)
            return FALSE;

        host = g_strndup(str + 1, close_bracket - str - 1);

        if (close_bracket[1] == ':') {
            port_str = g_strdup(close_bracket + 2);
        } else if (close_bracket[1] != '\0') {
            g_free(host);
            return FALSE;
        }
    } else {
        gchar **parts = g_strsplit(str, ":", 2);
        if (!parts || !parts[0]) {
            g_strfreev(parts);
            return FALSE;
        }

        host = g_strdup(parts[0]);
        if (parts[1] && *parts[1])
            port_str = g_strdup(parts[1]);

        g_strfreev(parts);
    }

    gboolean valid_host = awg_validate_ip4(host) || awg_validate_ip6(host) || awg_validate_fqdn(host);

    gboolean valid_port = TRUE;
    if (port_str) {
        valid_port = g_ascii_string_to_unsigned(port_str, 10, 0, 65535, NULL, NULL);
    } else if (has_colon) {
        valid_port = FALSE;
    }

    g_free(host);
    g_free(port_str);

    return valid_host && valid_port;
}

gboolean
awg_validate_fw_mark(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;
    return g_ascii_string_to_unsigned(str, 10, 0, UINT32_MAX, NULL, NULL);
}
