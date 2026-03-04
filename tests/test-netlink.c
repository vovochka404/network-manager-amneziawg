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
#include <glib.h>
#include <stdio.h>

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
    g_print("Listen port: %d\n", awg_device_get_listen_port(device));
    g_print("JC: %d, JMin: %d, JMax: %d\n", awg_device_get_jc(device), awg_device_get_jmin(device), awg_device_get_jmax(device));
    g_print("S1: %d, S2: %d\n", awg_device_get_s1(device), awg_device_get_s2(device));
    g_print("H1: %d, H2: %d, H3: %d, H4: %d\n",
            awg_device_get_h1(device) ?: -1,
            awg_device_get_h2(device) ?: -1,
            awg_device_get_h3(device) ?: -1,
            awg_device_get_h4(device) ?: -1);

    const GList *peers = awg_device_get_peers_list(device);
    if (peers) {
        AWGDevicePeer *peer = peers->data;
        g_print("Peer endpoint: %s\n", awg_device_peer_get_endpoint(peer) ?: "none");
        g_print("Peer allowed IPs: %s\n", awg_device_peer_get_allowed_ips_as_string(peer) ?: "none");
        g_print("Peer keepalive: %d\n", awg_device_peer_get_keep_alive_interval(peer));
        g_print("Peer advanced security: %d\n", awg_device_peer_get_advanced_security(peer));
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
