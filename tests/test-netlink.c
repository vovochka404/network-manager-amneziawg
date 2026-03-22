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

#include "amneziawg.h"
#include "awg/awg-config.h"
#include "awg/awg-connection-manager-netlink.h"
#include "awg/awg-connection-manager.h"
#include "awg/awg-device.h"
#include "awg/awg-validate.h"
#include <glib.h>
#include <stdio.h>

static gboolean
validate_config(AWGDevice *device)
{
    g_print("\n=== Validating configuration ===\n");

    if (!awg_device_is_valid(device)) {
        g_printerr("ERROR: Device validation failed (missing private key or peers)\n");
        return FALSE;
    }
    g_print("  Device: OK\n");

    if (!awg_validate_port(g_strdup_printf("%d", awg_device_get_listen_port(device)))) {
        g_printerr("ERROR: Invalid listen port\n");
        return FALSE;
    }
    g_print("  Listen port: OK\n");

    if (!awg_validate_mtu(g_strdup_printf("%d", awg_device_get_mtu(device)))) {
        g_printerr("ERROR: Invalid MTU\n");
        return FALSE;
    }
    g_print("  MTU: OK\n");

    if (!awg_validate_fw_mark(g_strdup_printf("%d", awg_device_get_fw_mark(device)))) {
        g_printerr("ERROR: Invalid FwMark\n");
        return FALSE;
    }
    g_print("  FwMark: OK\n");

    if (!awg_validate_jc(g_strdup_printf("%d", awg_device_get_jc(device)))) {
        g_printerr("ERROR: Invalid JC\n");
        return FALSE;
    }
    g_print("  JC: OK\n");

    if (!awg_validate_jmin(g_strdup_printf("%d", awg_device_get_jmin(device)))) {
        g_printerr("ERROR: Invalid JMin\n");
        return FALSE;
    }
    g_print("  JMin: OK\n");

    if (!awg_validate_jmax(g_strdup_printf("%d", awg_device_get_jmax(device)))) {
        g_printerr("ERROR: Invalid JMax\n");
        return FALSE;
    }
    g_print("  JMax: OK\n");

    if (!awg_validate_junk_size(g_strdup_printf("%d", awg_device_get_s1(device)))) {
        g_printerr("ERROR: Invalid S1\n");
        return FALSE;
    }
    g_print("  S1: OK\n");

    if (!awg_validate_junk_size(g_strdup_printf("%d", awg_device_get_s2(device)))) {
        g_printerr("ERROR: Invalid S2\n");
        return FALSE;
    }
    g_print("  S2: OK\n");

    if (!awg_validate_s3(g_strdup_printf("%d", awg_device_get_s3(device)))) {
        g_printerr("ERROR: Invalid S3 (must be <= 65479)\n");
        return FALSE;
    }
    g_print("  S3: OK\n");

    if (!awg_validate_s4(g_strdup_printf("%d", awg_device_get_s4(device)))) {
        g_printerr("ERROR: Invalid S4 (must be <= 65503)\n");
        return FALSE;
    }
    g_print("  S4: OK\n");

    if (!awg_validate_magic_header(awg_device_get_h1(device))) {
        g_printerr("ERROR: Invalid H1 format (expected: number or range like '1' or '1-100')\n");
        return FALSE;
    }
    g_print("  H1: OK\n");

    if (!awg_validate_magic_header(awg_device_get_h2(device))) {
        g_printerr("ERROR: Invalid H2 format\n");
        return FALSE;
    }
    g_print("  H2: OK\n");

    if (!awg_validate_magic_header(awg_device_get_h3(device))) {
        g_printerr("ERROR: Invalid H3 format\n");
        return FALSE;
    }
    g_print("  H3: OK\n");

    if (!awg_validate_magic_header(awg_device_get_h4(device))) {
        g_printerr("ERROR: Invalid H4 format\n");
        return FALSE;
    }
    g_print("  H4: OK\n");

    if (!awg_validate_i_packet(awg_device_get_i1(device))) {
        g_printerr("ERROR: Invalid I1 format\n");
        return FALSE;
    }
    g_print("  I1: OK\n");

    if (!awg_validate_i_packet(awg_device_get_i2(device))) {
        g_printerr("ERROR: Invalid I2 format\n");
        return FALSE;
    }
    g_print("  I2: OK\n");

    if (!awg_validate_i_packet(awg_device_get_i3(device))) {
        g_printerr("ERROR: Invalid I3 format\n");
        return FALSE;
    }
    g_print("  I3: OK\n");

    if (!awg_validate_i_packet(awg_device_get_i4(device))) {
        g_printerr("ERROR: Invalid I4 format\n");
        return FALSE;
    }
    g_print("  I4: OK\n");

    if (!awg_validate_i_packet(awg_device_get_i5(device))) {
        g_printerr("ERROR: Invalid I5 format\n");
        return FALSE;
    }
    g_print("  I5: OK\n");

    const GList *peers = awg_device_get_peers_list(device);
    gint peer_num = 1;
    while (peers) {
        AWGDevicePeer *peer = peers->data;

        g_print("\n  Peer %d:\n", peer_num);

        if (!awg_device_peer_is_valid(peer)) {
            g_printerr("ERROR: Peer %d is invalid\n", peer_num);
            return FALSE;
        }
        g_print("    Peer valid: OK\n");

        if (!awg_validate_base64(awg_device_peer_get_public_key(peer))) {
            g_printerr("ERROR: Peer %d: invalid public key\n", peer_num);
            return FALSE;
        }
        g_print("    Public key: OK\n");

        if (!awg_validate_endpoint(awg_device_peer_get_endpoint(peer))) {
            g_printerr("ERROR: Peer %d: invalid endpoint\n", peer_num);
            return FALSE;
        }
        g_print("    Endpoint: OK\n");

        if (!awg_validate_allowed_ips(awg_device_peer_get_allowed_ips_as_string(peer))) {
            g_printerr("ERROR: Peer %d: invalid allowed IPs\n", peer_num);
            return FALSE;
        }
        g_print("    Allowed IPs: OK\n");

        peers = peers->next;
        peer_num++;
    }

    g_print("\n  All validations passed!\n");
    return TRUE;
}

static void
test_netlink_connect(const gchar *config_path)
{
    AWGDevice *device = awg_device_new_from_config(config_path);

    if (!device) {
        g_printerr("Failed to load config from %s\n", config_path);
        return;
    }

    g_print("Config loaded successfully\n");
    g_print("Private key: %s\n", awg_device_get_private_key(device) ? awg_device_get_private_key(device) : "none");
    g_print("Address v4: %s\n", awg_device_get_address_v4(device) ? g_inet_address_to_string((GInetAddress *)awg_device_get_address_v4(device)) : "none");
    g_print("Listen port: %d, MTU: %d, FwMark: 0x%x\n",
            awg_device_get_listen_port(device),
            awg_device_get_mtu(device),
            awg_device_get_fw_mark(device));
    g_print("JC: %d, JMin: %d, JMax: %d\n", awg_device_get_jc(device), awg_device_get_jmin(device), awg_device_get_jmax(device));
    g_print("S1: %d, S2: %d, S3: %d, S4: %d\n",
            awg_device_get_s1(device),
            awg_device_get_s2(device),
            awg_device_get_s3(device),
            awg_device_get_s4(device));
    g_print("H1: %s, H2: %s, H3: %s, H4: %s\n",
            awg_device_get_h1(device) ?: "(none)",
            awg_device_get_h2(device) ?: "(none)",
            awg_device_get_h3(device) ?: "(none)",
            awg_device_get_h4(device) ?: "(none)");
    g_print("I1: %s\n", awg_device_get_i1(device) ?: "(none)");
    g_print("I2: %s\n", awg_device_get_i2(device) ?: "(none)");
    g_print("I3: %s\n", awg_device_get_i3(device) ?: "(none)");
    g_print("I4: %s\n", awg_device_get_i4(device) ?: "(none)");
    g_print("I5: %s\n", awg_device_get_i5(device) ?: "(none)");

    const GList *peers = awg_device_get_peers_list(device);
    if (peers) {
        AWGDevicePeer *peer = peers->data;
        g_print("Peer endpoint: %s\n", awg_device_peer_get_endpoint(peer) ?: "none");
        g_print("Peer allowed IPs: %s\n", awg_device_peer_get_allowed_ips_as_string(peer) ?: "none");
        g_print("Peer keepalive: %d\n", awg_device_peer_get_keep_alive_interval(peer));
        g_print("Peer advanced security: %d\n", awg_device_peer_get_advanced_security(peer));
    }

    if (!validate_config(device)) {
        g_object_unref(device);
        return;
    }

    const gchar *ifname = "testawg0";

    g_print("\n=== Testing netlink connection manager ===\n");

    if (!awg_connection_manager_netlink_is_available()) {
        g_printerr("Netlink connection manager is not available (no kernel module?)\n");
        g_object_unref(device);
        return;
    }

    g_print("Netlink is available, creating connection manager...\n");

    AWGConnectionManager *mgr = awg_connection_manager_netlink_new(ifname, device);
    if (!mgr) {
        g_printerr("Failed to create connection manager\n");
        g_object_unref(device);
        return;
    }

    GError *error = NULL;
    g_print("Attempting to connect...\n");

    if (!awg_connection_manager_connect(mgr, &error)) {
        g_printerr("Failed to connect: %s\n", error ? error->message : "unknown error");
        if (error)
            g_error_free(error);
    } else {
        g_print("Connected successfully!\n");
        g_print("Interface %s is now up\n", ifname);

        g_print("Press Enter to disconnect...");
        getchar();

        if (!awg_connection_manager_disconnect(mgr, &error)) {
            g_printerr("Failed to disconnect: %s\n", error ? error->message : "unknown error");
            if (error)
                g_error_free(error);
        } else {
            g_print("Disconnected successfully\n");
        }
    }

    g_object_unref(mgr);
    g_object_unref(device);
    wg_del_device(ifname);
}

int
main(int argc, char *argv[])
{
    g_print("=== AmneziaWG Netlink Test ===\n");
    g_print("This test will try to create and configure an AmneziaWG interface\n");
    g_print("Run as root to test netlink functionality!\n\n");

    if (argc < 2) {
        g_printerr("Usage: %s <config-file>\n", argv[0]);
        return 1;
    }

    test_netlink_connect(argv[1]);

    g_print("\nTest complete.\n");
    return 0;
}
