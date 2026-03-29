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

#define MAX_AWG_STRING_LEN (5 * 1024)

gboolean
awg_validate_awg_string(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;
    return strlen(str) <= MAX_AWG_STRING_LEN;
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
awg_validate_jmin(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;
    return g_ascii_string_to_unsigned(str, 10, AWG_JMIN_MIN, AWG_JMIN_MAX, NULL, NULL);
}

gboolean
awg_validate_jmax(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;
    return g_ascii_string_to_unsigned(str, 10, AWG_JMAX_MIN, AWG_JMAX_MAX, NULL, NULL);
}

gboolean
awg_validate_junk_size(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;
    return g_ascii_string_to_unsigned(str, 10, AWG_JUNK_SIZE_MIN, AWG_JUNK_SIZE_MAX, NULL, NULL);
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

gboolean
awg_validate_magic_header(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;

    const gchar *dash = strchr(str, '-');

    if (dash) {
        guint64 start, end;
        gchar *start_str = g_strndup(str, dash - str);
        gchar *end_str = g_strdup(dash + 1);

        gboolean start_ok = g_ascii_string_to_unsigned(start_str, 10, 0, G_MAXUINT32, &start, NULL);
        gboolean end_ok = g_ascii_string_to_unsigned(end_str, 10, 0, G_MAXUINT32, &end, NULL);

        g_free(start_str);
        g_free(end_str);

        if (!start_ok || !end_ok)
            return FALSE;

        return start <= end;
    } else {
        guint64 value;
        return g_ascii_string_to_unsigned(str, 10, 0, G_MAXUINT32, &value, NULL);
    }
}

gboolean
awg_magic_header_parse(const gchar *str, guint32 *start, guint32 *end)
{
    if (!str || !*str) {
        *start = 0;
        *end = 0;
        return FALSE;
    }

    const gchar *dash = strchr(str, '-');

    if (dash) {
        gchar *start_str = g_strndup(str, dash - str);
        gchar *end_str = g_strdup(dash + 1);
        guint64 start_val, end_val;

        gboolean start_ok = g_ascii_string_to_unsigned(start_str, 10, 0, G_MAXUINT64, &start_val, NULL);
        gboolean end_ok = g_ascii_string_to_unsigned(end_str, 10, 0, G_MAXUINT64, &end_val, NULL);

        g_free(start_str);
        g_free(end_str);

        if (!start_ok || !end_ok || start_val > G_MAXUINT32 || end_val > G_MAXUINT32)
            return FALSE;

        *start = (guint32)start_val;
        *end = (guint32)end_val;
        return *start <= *end;
    } else {
        guint64 value;
        gboolean ok = g_ascii_string_to_unsigned(str, 10, 0, G_MAXUINT64, &value, NULL);
        if (!ok || value > G_MAXUINT32)
            return FALSE;
        *start = (guint32)value;
        *end = (guint32)value;
        return TRUE;
    }
}

gboolean
awg_validate_magic_headers_no_overlap(const gchar *h1, const gchar *h2, const gchar *h3, const gchar *h4)
{
    guint32 starts[4];
    guint32 ends[4];
    gboolean has_value[4] = { FALSE, FALSE, FALSE, FALSE };

    has_value[0] = awg_magic_header_parse(h1, &starts[0], &ends[0]);
    has_value[1] = awg_magic_header_parse(h2, &starts[1], &ends[1]);
    has_value[2] = awg_magic_header_parse(h3, &starts[2], &ends[2]);
    has_value[3] = awg_magic_header_parse(h4, &starts[3], &ends[3]);

    for (gint i = 0; i < 4; i++) {
        for (gint j = i + 1; j < 4; j++) {
            if (has_value[i] && has_value[j]) {
                if (!(ends[j] < starts[i] || ends[i] < starts[j])) {
                    return FALSE;
                }
            }
        }
    }

    return TRUE;
}

static gboolean
validate_i_tag_format(const gchar *tag_str, gint tag_len, const gchar *value_str, gint value_len)
{
    if (tag_len < 1)
        return FALSE;

    if (tag_str[0] == 'c' || tag_str[0] == 't') {
        return (tag_len == 1 && value_len == 0);
    }

    if (tag_str[0] == 'b') {
        if (tag_len != 1)
            return FALSE;
        if (value_len < 2 || value_str[0] != '0' || value_str[1] != 'x')
            return FALSE;
        for (gint i = 2; i < value_len; i++) {
            if (!g_ascii_isxdigit(value_str[i]))
                return FALSE;
        }
        return ((value_len - 2) % 2) == 0;
    }

    if (tag_str[0] == 'r') {
        if (tag_len == 1) {
            guint64 value;
            if (!g_ascii_string_to_unsigned(value_str, 10, 0, G_MAXUINT16, &value, NULL))
                return FALSE;
            return TRUE;
        }
        if (tag_len == 2 && (tag_str[1] == 'c' || tag_str[1] == 'd')) {
            guint64 value;
            if (!g_ascii_string_to_unsigned(value_str, 10, 0, G_MAXUINT16, &value, NULL))
                return FALSE;
            return TRUE;
        }
    }

    return FALSE;
}

gboolean
awg_validate_i_packet(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;

    if (strlen(str) > MAX_AWG_STRING_LEN)
        return FALSE;

    gchar **tokens = g_strsplit(str, "<", -1);
    if (!tokens)
        return FALSE;

    gboolean valid = TRUE;

    for (gint i = 0; tokens[i] && valid; i++) {
        gchar *token = tokens[i];
        while (*token == ' ')
            token++;

        gint token_len = strlen(token);
        while (token_len > 0 && token[token_len - 1] == ' ') {
            token_len--;
            token[token_len] = '\0';
        }

        if (token_len == 0)
            continue;

        if (token[token_len - 1] == '>')
            token[--token_len] = '\0';

        if (token_len == 0)
            continue;

        gchar *space = strchr(token, ' ');
        gchar *tag;
        gint tag_len;
        const gchar *value;
        gint value_len;

        if (space) {
            tag = token;
            tag_len = space - token;
            value = space + 1;
            value_len = token + token_len - (space + 1);
        } else {
            tag = token;
            tag_len = token_len;
            value = "";
            value_len = 0;
        }

        if (!validate_i_tag_format(tag, tag_len, value, value_len)) {
            valid = FALSE;
            break;
        }
    }

    g_strfreev(tokens);
    return valid;
}

gboolean
awg_validate_s3(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;
    return g_ascii_string_to_unsigned(str, 10, 0, AWG_S3_MAX, NULL, NULL);
}

gboolean
awg_validate_s4(const gchar *str)
{
    if (awg_validate_empty(str))
        return TRUE;
    return g_ascii_string_to_unsigned(str, 10, 0, AWG_S4_MAX, NULL, NULL);
}

gboolean
awg_validate_jmin_jmax(guint16 jmin, guint16 jmax)
{
    if (jmin == 0 && jmax == 0)
        return TRUE;
    return jmin <= jmax;
}
