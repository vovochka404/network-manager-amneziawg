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

#include "awg-config.h"
#include "awg-device.h"
#include <glib.h>

AWGDevice *
awg_device_new_from_config(const char *config_path)
{
    GError *error = NULL;
    GDataInputStream *data_input_stream;
    gchar *line, *key, *value;
    gboolean is_interface_section = FALSE, is_peer_section = FALSE;
    AWGDevice *device = NULL;
    AWGDevicePeer *peer = NULL;
    gchar **parts = NULL;
    gboolean success = TRUE;
    GFile *file = g_file_new_for_path(config_path);
    GInputStream *input_stream = (GInputStream *)g_file_read(file, NULL, &error);

    if (error) {
        g_warning("Failed to open file: %s", error->message);
        g_error_free(error);
        g_object_unref(file);
        return NULL;
    }

    device = awg_device_new();
    data_input_stream = g_data_input_stream_new(input_stream);

    while ((line = g_data_input_stream_read_line_utf8(data_input_stream, NULL, NULL, &error)) != NULL) {
        line = g_strstrip(line);
        if (!*line || (*line == '#' && !g_str_has_prefix(line, AWG_CONFIG_DEVICE_PUBLIC_KEY_COMMENTED))) {
            continue;
        }

        if (g_strcmp0(line, AWG_CONFIG_INTERFACE_SECTION) == 0) {
            is_interface_section = TRUE;
            is_peer_section = FALSE;
            continue;
        }

        if (g_strcmp0(line, AWG_CONFIG_PEER_SECTION) == 0) {
            is_interface_section = FALSE;
            is_peer_section = TRUE;

            if (peer) {
                awg_device_add_peer(device, peer);
                g_object_unref(peer);
            }
            peer = awg_device_peer_new();
            continue;
        }

        parts = g_strsplit(line, "=", 2);
        if (!(parts && parts[0] && parts[1])) {
            g_warning("Invalid config line: %s", line);
            g_strfreev(parts);
            continue;
        }

        key = g_strstrip(parts[0]);
        value = g_strstrip(parts[1]);

        if (is_interface_section) {
            if (g_strcmp0(key, AWG_CONFIG_DEVICE_PRIVATE_KEY) == 0) {
                success &= awg_device_set_private_key(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_PUBLIC_KEY) == 0 || g_strcmp0(key, AWG_CONFIG_DEVICE_PUBLIC_KEY_COMMENTED) == 0) {
                success &= awg_device_set_public_key(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_ADDRESS) == 0) {
                success &= awg_device_set_address_from_string(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_LISTEN_PORT) == 0) {
                success &= awg_device_set_listen_port_from_string(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_DNS) == 0) {
                success &= awg_device_set_dns_from_string(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_FW_MARK) == 0) {
                success &= awg_device_set_fw_mark_from_string(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_JC) == 0) {
                success &= awg_device_set_jc_from_string(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_JMIN) == 0) {
                success &= awg_device_set_jmin_from_string(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_JMAX) == 0) {
                success &= awg_device_set_jmax_from_string(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_S1) == 0) {
                success &= awg_device_set_s1_from_string(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_S2) == 0) {
                success &= awg_device_set_s2_from_string(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_S3) == 0) {
                success &= awg_device_set_s3_from_string(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_S4) == 0) {
                success &= awg_device_set_s4_from_string(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_H1) == 0) {
                success &= awg_device_set_h1(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_H2) == 0) {
                success &= awg_device_set_h2(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_H3) == 0) {
                success &= awg_device_set_h3(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_H4) == 0) {
                success &= awg_device_set_h4(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_I1) == 0) {
                success &= awg_device_set_i1(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_I2) == 0) {
                success &= awg_device_set_i2(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_I3) == 0) {
                success &= awg_device_set_i3(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_I4) == 0) {
                success &= awg_device_set_i4(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_I5) == 0) {
                success &= awg_device_set_i5(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_MTU) == 0) {
                success &= awg_device_set_mtu_from_string(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_PRE_UP) == 0) {
                awg_device_set_pre_up(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_POST_UP) == 0) {
                awg_device_set_post_up(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_PRE_DOWN) == 0) {
                awg_device_set_pre_down(device, value);
            } else if (g_strcmp0(key, AWG_CONFIG_DEVICE_POST_DOWN) == 0) {
                awg_device_set_post_down(device, value);
            }
        } else if (is_peer_section) {
            if (g_strcmp0(key, AWG_CONFIG_PEER_ENDPOINT) == 0) {
                success &= awg_device_peer_set_endpoint(peer, value);
            } else if (g_strcmp0(key, AWG_CONFIG_PEER_PUBLIC_KEY) == 0) {
                success &= awg_device_peer_set_public_key(peer, value);
            } else if (g_strcmp0(key, AWG_CONFIG_PEER_PRESHARED_KEY) == 0) {
                success &= awg_device_peer_set_shared_key(peer, value);
            } else if (g_strcmp0(key, AWG_CONFIG_PEER_ALLOWED_IPS) == 0) {
                success &= awg_device_peer_set_allowed_ips_from_string(peer, value);
            } else if (g_strcmp0(key, AWG_CONFIG_PEER_KEEP_ALIVE) == 0) {
                success &= awg_device_peer_set_keep_alive_interval_from_string(peer, value);
            } else if (g_strcmp0(key, AWG_CONFIG_PEER_ADVANCED_SECURITY) == 0) {
                if (g_strcmp0(value, "on") == 0) {
                    awg_device_peer_set_advanced_security(peer, TRUE);
                } else if (g_strcmp0(value, "off") == 0) {
                    awg_device_peer_set_advanced_security(peer, FALSE);
                } else {
                    g_warning("Invalid peer advanced security value: %s (on / off allowed)", value);
                    success = FALSE;
                }
            }
        }
    }

    if (error) {
        g_warning("Error reading file: %s", error->message);
        g_error_free(error);
    }

    if (peer) {
        awg_device_add_peer(device, peer);
        g_object_unref(peer);
    }

    if (parts) {
        g_strfreev(parts);
    }

    g_object_unref(data_input_stream);
    g_object_unref(input_stream);
    g_object_unref(file);

    if (!success || !awg_device_is_valid(device)) {
        g_warning("Invalid AWG device configuration.");
        g_clear_object(&device);
    }

    return device;
}

gchar *
awg_device_create_config_string(AWGDevice *device)
{
    const gchar *value = NULL;
    guint16 uint_value = 0;
    AWGDevicePeer *peer = NULL;
    const GInetAddress *address = NULL;
    const GList *iter = NULL;
    GString *dns_str = NULL;
    gchar *addr_str = NULL;
    GString *config = g_string_new(AWG_CONFIG_INTERFACE_SECTION);
    g_string_append(config, "\n");

    // Append device settings
    value = awg_device_get_private_key(device);
    if (value) {
        g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_PRIVATE_KEY, value);
    }

    value = awg_device_get_public_key(device);
    if (value) {
        g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_PUBLIC_KEY_COMMENTED, value);
    }

    address = awg_device_get_address_v4(device);
    if (address) {
        addr_str = g_inet_address_to_string((GInetAddress *)address);
        if (g_inet_address_get_family((GInetAddress *)address) == G_SOCKET_FAMILY_IPV6) {
            g_string_append_printf(config, "%s = [%s]/128\n", AWG_CONFIG_DEVICE_ADDRESS, addr_str);
        } else {
            g_string_append_printf(config, "%s = %s/32\n", AWG_CONFIG_DEVICE_ADDRESS, addr_str);
        }
        g_clear_pointer(&addr_str, g_free);
    }

    uint_value = awg_device_get_listen_port(device);
    if (uint_value) {
        g_string_append_printf(config, "%s = %u\n", AWG_CONFIG_DEVICE_LISTEN_PORT, uint_value);
    }

    iter = awg_device_get_dns_v4_list(device);
    if (iter) {
        dns_str = g_string_new("");
        for (; iter != NULL; iter = iter->next) {
            address = (const GInetAddress *)iter->data;
            addr_str = g_inet_address_to_string((GInetAddress *)address);
            g_string_append_printf(dns_str, "%s, ", addr_str);
            g_clear_pointer(&addr_str, g_free);
        }
    }

    iter = awg_device_get_dns_v6_list(device);
    if (iter) {
        if (!dns_str) {
            dns_str = g_string_new("");
        }
        for (; iter != NULL; iter = iter->next) {
            address = (const GInetAddress *)iter->data;
            addr_str = g_inet_address_to_string((GInetAddress *)address);
            g_string_append_printf(dns_str, "%s, ", addr_str);
            g_clear_pointer(&addr_str, g_free);
        }
    }

    if (dns_str) {
        if (dns_str->len > 0) {
            g_string_set_size(dns_str, dns_str->len - 2);
            g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_DNS, dns_str->str);
        }
        g_string_free(dns_str, TRUE);
    }

    uint_value = awg_device_get_fw_mark(device);
    if (uint_value) {
        g_string_append_printf(config, "%s = %u\n", AWG_CONFIG_DEVICE_FW_MARK, uint_value);
    }

    guint32 mtu = awg_device_get_mtu(device);
    if (mtu > 0) {
        g_string_append_printf(config, "%s = %u\n", AWG_CONFIG_DEVICE_MTU, mtu);
    }

    g_string_append_printf(config, "%s = %u\n", AWG_CONFIG_DEVICE_JC, awg_device_get_jc(device));
    g_string_append_printf(config, "%s = %u\n", AWG_CONFIG_DEVICE_JMIN, awg_device_get_jmin(device));
    g_string_append_printf(config, "%s = %u\n", AWG_CONFIG_DEVICE_JMAX, awg_device_get_jmax(device));
    g_string_append_printf(config, "%s = %u\n", AWG_CONFIG_DEVICE_S1, awg_device_get_s1(device));
    g_string_append_printf(config, "%s = %u\n", AWG_CONFIG_DEVICE_S2, awg_device_get_s2(device));
    g_string_append_printf(config, "%s = %u\n", AWG_CONFIG_DEVICE_S3, awg_device_get_s3(device));
    g_string_append_printf(config, "%s = %u\n", AWG_CONFIG_DEVICE_S4, awg_device_get_s4(device));

    const gchar *h1 = awg_device_get_h1(device);
    if (h1 && *h1) {
        g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_H1, h1);
    }
    const gchar *h2 = awg_device_get_h2(device);
    if (h2 && *h2) {
        g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_H2, h2);
    }
    const gchar *h3 = awg_device_get_h3(device);
    if (h3 && *h3) {
        g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_H3, h3);
    }
    const gchar *h4 = awg_device_get_h4(device);
    if (h4 && *h4) {
        g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_H4, h4);
    }

    const gchar *i1 = awg_device_get_i1(device);
    if (i1 && *i1) {
        g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_I1, i1);
    }
    const gchar *i2 = awg_device_get_i2(device);
    if (i2 && *i2) {
        g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_I2, i2);
    }
    const gchar *i3 = awg_device_get_i3(device);
    if (i3 && *i3) {
        g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_I3, i3);
    }
    const gchar *i4 = awg_device_get_i4(device);
    if (i4 && *i4) {
        g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_I4, i4);
    }
    const gchar *i5 = awg_device_get_i5(device);
    if (i5 && *i5) {
        g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_I5, i5);
    }

    const gchar *pre_up = awg_device_get_pre_up(device);
    if (pre_up && *pre_up) {
        g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_PRE_UP, pre_up);
    }

    const gchar *post_up = awg_device_get_post_up(device);
    if (post_up && *post_up) {
        g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_POST_UP, post_up);
    }

    const gchar *pre_down = awg_device_get_pre_down(device);
    if (pre_down && *pre_down) {
        g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_PRE_DOWN, pre_down);
    }

    const gchar *post_down = awg_device_get_post_down(device);
    if (post_down && *post_down) {
        g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_DEVICE_POST_DOWN, post_down);
    }

    // Append peer settings
    iter = awg_device_get_peers_list(device);
    if (iter) {
        for (; iter != NULL; iter = iter->next) {
            g_string_append(config, "\n");
            g_string_append(config, AWG_CONFIG_PEER_SECTION);
            g_string_append(config, "\n");

            peer = (AWGDevicePeer *)iter->data;
            value = awg_device_peer_get_public_key(peer);
            if (value) {
                g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_PEER_PUBLIC_KEY, value);
            }
            value = awg_device_peer_get_shared_key(peer);
            if (value) {
                g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_PEER_PRESHARED_KEY, value);
            }
            value = awg_device_peer_get_endpoint(peer);
            if (value) {
                g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_PEER_ENDPOINT, value);
            }
            uint_value = awg_device_peer_get_keep_alive_interval(peer);
            if (uint_value) {
                g_string_append_printf(config, "%s = %u\n", AWG_CONFIG_PEER_KEEP_ALIVE, uint_value);
            } else {
                g_string_append_printf(config, "%s = off\n", AWG_CONFIG_PEER_KEEP_ALIVE);
            }
            value = awg_device_peer_get_allowed_ips_as_string(peer);
            if (value) {
                g_string_append_printf(config, "%s = %s\n", AWG_CONFIG_PEER_ALLOWED_IPS, value);
            }
            if (awg_device_peer_get_advanced_security(peer)) {
                g_string_append_printf(config, "%s = on\n", AWG_CONFIG_PEER_ADVANCED_SECURITY);
            }
        }
    }

    return g_string_free(config, FALSE);
}

gboolean
awg_device_save_to_file(AWGDevice *device, const char *config_path)
{
    GError *error = NULL;
    gchar *content = NULL;
    GFile *file = g_file_new_for_path(config_path);
    GOutputStream *output_stream = (GOutputStream *)g_file_replace(file, NULL, FALSE, G_FILE_CREATE_REPLACE_DESTINATION, NULL, &error);

    if (error) {
        g_warning("Failed to open file: %s", error->message);
        g_error_free(error);
        g_object_unref(file);
        return FALSE;
    }

    content = awg_device_create_config_string(device);
    if (!content) {
        g_object_unref(output_stream);
        g_object_unref(file);
        return FALSE;
    }

    if (!g_output_stream_write_all(output_stream, content, strlen(content), NULL, NULL, &error)) {
        g_warning("Failed to write to file: %s", error->message);
        g_error_free(error);
        g_object_unref(output_stream);
        g_object_unref(file);
        g_free(content);
        return FALSE;
    }

    g_object_unref(output_stream);
    g_object_unref(file);
    g_free(content);

    return TRUE;
}