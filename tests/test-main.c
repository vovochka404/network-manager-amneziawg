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

#include "awg/awg-config.h"
#include "awg/awg-connection-manager-external.h"
#include "awg/awg-connection-manager-netlink.h"
#include "awg/awg-connection-manager.h"
#include "awg/awg-device.h"
#include "awg/awg-nm-connection.h"
#include "awg/awg-secrets.h"
#include "awg/awg-validate.h"
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static gchar *
get_test_config_path(const gchar *filename)
{
    return g_build_filename(g_get_current_dir(), "tests", filename, NULL);
}

static void
test_awg_device_new_from_config_ipv4(void)
{
    gchar *config_path = get_test_config_path("test-config-ipv4.conf");
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);

    g_assert_nonnull(device);
    g_assert_cmpint(awg_device_get_peers_count(device), ==, 1);

    const GInetAddress *addr_v4 = awg_device_get_address_v4(device);
    g_assert_nonnull(addr_v4);
    g_assert_true(g_inet_address_get_family((GInetAddress *)addr_v4) == G_SOCKET_FAMILY_IPV4);

    const GInetAddress *addr_v6 = awg_device_get_address_v6(device);
    g_assert_null(addr_v6);

    g_assert_cmpint(awg_device_get_dns_v4_count(device), ==, 2);
    g_assert_cmpint(awg_device_get_listen_port(device), ==, 51820);
    g_assert_cmpint(awg_device_get_jc(device), ==, 3);
    g_assert_cmpint(awg_device_get_jmin(device), ==, 15);
    g_assert_cmpint(awg_device_get_jmax(device), ==, 30);
    g_assert_cmpint(awg_device_get_s1(device), ==, 10);
    g_assert_cmpint(awg_device_get_s2(device), ==, 20);
    g_assert_cmpstr(awg_device_get_h1(device), ==, "1");
    g_assert_cmpstr(awg_device_get_h2(device), ==, "2");
    g_assert_cmpstr(awg_device_get_h3(device), ==, "3");
    g_assert_cmpstr(awg_device_get_h4(device), ==, "4");
    g_assert_cmpint(awg_device_get_mtu(device), ==, 1420);

    const GList *peers = awg_device_get_peers_list(device);
    g_assert_nonnull(peers);
    AWGDevicePeer *peer = peers->data;
    g_assert_cmpstr(awg_device_peer_get_endpoint(peer), ==, "192.168.1.1:51820");
    g_assert_cmpint(awg_device_peer_get_keep_alive_interval(peer), ==, 25);
    g_assert_true(awg_device_peer_get_advanced_security(peer));

    g_assert_true(awg_device_peer_set_keep_alive_interval(peer, G_MAXUINT16));
    g_assert_cmpint(awg_device_peer_get_keep_alive_interval(peer), ==, G_MAXUINT16);

    g_object_unref(device);
    g_free(config_path);
}

static void
test_awg_i_packet_requires_closing_delimiter(void)
{
    g_assert_true(awg_validate_i_packet("<r 16><c>"));
    g_assert_false(awg_validate_i_packet("<r 16"));
    g_assert_false(awg_validate_i_packet("<c"));
    g_assert_false(awg_validate_i_packet("<t"));
}

static void
test_awg_device_new_from_config_ipv6(void)
{
    gchar *config_path = get_test_config_path("test-config-ipv6.conf");
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);

    g_assert_nonnull(device);

    const GInetAddress *addr_v4 = awg_device_get_address_v4(device);
    g_assert_null(addr_v4);

    const GInetAddress *addr_v6 = awg_device_get_address_v6(device);
    g_assert_nonnull(addr_v6);
    g_assert_true(g_inet_address_get_family((GInetAddress *)addr_v6) == G_SOCKET_FAMILY_IPV6);

    g_assert_cmpint(awg_device_get_dns_v6_count(device), ==, 2);
    g_assert_cmpint(awg_device_get_listen_port(device), ==, 51820);

    const GList *peers = awg_device_get_peers_list(device);
    g_assert_nonnull(peers);
    AWGDevicePeer *peer = peers->data;
    g_assert_cmpstr(awg_device_peer_get_endpoint(peer), ==, "[2001:db8::1]:51820");

    g_object_unref(device);
    g_free(config_path);
}

static void
test_awg_device_new_from_config_dual_stack(void)
{
    gchar *config_path = get_test_config_path("test-config-dual.conf");
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);

    g_assert_nonnull(device);

    const GInetAddress *addr_v4 = awg_device_get_address_v4(device);
    g_assert_nonnull(addr_v4);
    g_assert_true(g_inet_address_get_family((GInetAddress *)addr_v4) == G_SOCKET_FAMILY_IPV4);

    const GInetAddress *addr_v6 = awg_device_get_address_v6(device);
    g_assert_nonnull(addr_v6);
    g_assert_true(g_inet_address_get_family((GInetAddress *)addr_v6) == G_SOCKET_FAMILY_IPV6);

    g_assert_cmpint(awg_device_get_dns_v4_count(device), ==, 2);
    g_assert_cmpint(awg_device_get_dns_v6_count(device), ==, 2);

    const GList *peers = awg_device_get_peers_list(device);
    g_assert_nonnull(peers);
    AWGDevicePeer *peer = peers->data;
    g_assert_cmpstr(awg_device_peer_get_endpoint(peer), ==, "vpn.example.com:51820");

    g_object_unref(device);
    g_free(config_path);
}

static void
test_awg_device_new_from_config_multi_peer(void)
{
    gchar *config_path = get_test_config_path("test-config-multi-peer.conf");
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);

    g_assert_nonnull(device);
    g_assert_cmpint(awg_device_get_peers_count(device), ==, 3);

    const GList *peers = awg_device_get_peers_list(device);
    g_assert_nonnull(peers);

    AWGDevicePeer *peer1 = peers->data;
    g_assert_cmpstr(awg_device_peer_get_endpoint(peer1), ==, "192.168.1.1:51820");
    g_assert_cmpint(awg_device_peer_get_keep_alive_interval(peer1), ==, 25);
    g_assert_true(awg_device_peer_get_advanced_security(peer1));
    g_assert_cmpstr(awg_device_peer_get_allowed_ips_as_string(peer1), ==, "10.0.0.0/24");

    AWGDevicePeer *peer2 = peers->next->data;
    g_assert_cmpstr(awg_device_peer_get_endpoint(peer2), ==, "[2001:db8::1]:51820");
    g_assert_cmpint(awg_device_peer_get_keep_alive_interval(peer2), ==, 0);
    g_assert_false(awg_device_peer_get_advanced_security(peer2));
    g_assert_cmpstr(awg_device_peer_get_allowed_ips_as_string(peer2), ==, "fd00::/8");

    AWGDevicePeer *peer3 = peers->next->next->data;
    g_assert_cmpstr(awg_device_peer_get_endpoint(peer3), ==, "vpn2.example.com:443");
    g_assert_cmpint(awg_device_peer_get_keep_alive_interval(peer3), ==, 30);
    g_assert_true(awg_device_peer_get_advanced_security(peer3));
    g_assert_cmpstr(awg_device_peer_get_allowed_ips_as_string(peer3), ==, "172.16.0.0/12");

    g_object_unref(device);
    g_free(config_path);
}

static void
test_awg_device_save_and_load_ipv4(void)
{
    gchar *config_path = get_test_config_path("test-config-ipv4.conf");
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);
    g_assert_nonnull(device);

    gchar *output_path = g_build_filename(g_get_tmp_dir(), "output-ipv4.conf", NULL);
    gboolean saved = awg_device_save_to_file(device, output_path);
    g_assert_true(saved);

    g_object_unref(device);

    AWGDevice *device2 = awg_device_new_from_config(output_path, NULL);
    g_assert_nonnull(device2);

    g_assert_cmpint(awg_device_get_peers_count(device2), ==, 1);
    g_assert_cmpint(awg_device_get_listen_port(device2), ==, 51820);
    g_assert_cmpint(awg_device_get_jc(device2), ==, 3);
    g_assert_cmpint(awg_device_get_jmin(device2), ==, 15);
    g_assert_cmpint(awg_device_get_jmax(device2), ==, 30);
    g_assert_cmpint(awg_device_get_mtu(device2), ==, 1420);

    const GList *peers = awg_device_get_peers_list(device2);
    AWGDevicePeer *peer = peers->data;
    g_assert_cmpstr(awg_device_peer_get_endpoint(peer), ==, "192.168.1.1:51820");
    g_assert_cmpint(awg_device_peer_get_keep_alive_interval(peer), ==, 25);

    g_object_unref(device2);

    unlink(output_path);
    g_free(output_path);
    g_free(config_path);
}

static void
test_awg_config_invalid_value_is_reported(void)
{
    /* The import path needs the reason in the error, not only in the journal. */
    gchar *config_path = get_test_config_path("test-config-invalid-value.conf");
    GError *error = NULL;
    AWGDevice *device;

    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*Invalid s1*");
    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*Invalid AWG device configuration*");
    device = awg_device_new_from_config(config_path, &error);
    g_test_assert_expected_messages();

    g_assert_null(device);
    g_assert_nonnull(error);
    g_assert_nonnull(strstr(error->message, "S1"));
    g_assert_nonnull(strstr(error->message, "999999"));

    g_clear_error(&error);
    g_free(config_path);
}

static void
test_awg_config_incomplete_is_reported(void)
{
    /* A parse that succeeds but yields an unusable device reports why. */
    gchar *config_path = get_test_config_path("test-config-no-keys.conf");
    GError *error = NULL;
    AWGDevice *device;

    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*Invalid AWG device configuration*");
    device = awg_device_new_from_config(config_path, &error);
    g_test_assert_expected_messages();

    g_assert_null(device);
    g_assert_nonnull(error);
    g_assert_nonnull(strstr(error->message, "PrivateKey"));

    g_clear_error(&error);
    g_free(config_path);
}

static void
test_awg_device_save_uses_private_permissions(void)
{
    gchar *config_path = get_test_config_path("test-config-extended.conf");
    gchar *output_path = g_build_filename(g_get_tmp_dir(), "output-private-perms.conf", NULL);
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);
    struct stat st;

    g_assert_nonnull(device);

    /* The generated configuration carries the private key, so it must never be
     * readable by anyone else — not even briefly, which is why it is created
     * with G_FILE_CREATE_PRIVATE instead of being chmod()ed afterwards. */
    g_assert_true(awg_device_save_to_file(device, output_path));
    g_assert_cmpint(stat(output_path, &st), ==, 0);
    g_assert_cmpuint(st.st_mode & 0777, ==, 0600);

    g_object_unref(device);
    unlink(output_path);
    g_free(output_path);
    g_free(config_path);
}

static void
test_awg_device_save_and_load_multi_peer(void)
{
    gchar *config_path = get_test_config_path("test-config-multi-peer.conf");
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);
    g_assert_nonnull(device);

    gchar *output_path = g_build_filename(g_get_tmp_dir(), "output-multi-peer.conf", NULL);
    gboolean saved = awg_device_save_to_file(device, output_path);
    g_assert_true(saved);

    g_object_unref(device);

    AWGDevice *device2 = awg_device_new_from_config(output_path, NULL);
    g_assert_nonnull(device2);
    g_assert_cmpint(awg_device_get_peers_count(device2), ==, 3);

    g_object_unref(device2);

    unlink(output_path);
    g_free(output_path);
    g_free(config_path);
}

static void
test_awg_device_mtu_not_exported_when_zero(void)
{
    AWGDevice *device = awg_device_new();
    g_assert_nonnull(device);

    g_assert_cmpint(awg_device_get_mtu(device), ==, 0);

    awg_device_set_address_from_string(device, "10.0.0.2/32");
    awg_device_set_private_key(device, "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20=");

    gchar *config_str = awg_device_create_config_string(device);
    g_assert_nonnull(config_str);

    g_assert_null(strstr(config_str, "MTU"));

    g_free(config_str);
    g_object_unref(device);
}

static void
test_awg_device_mtu_exported_when_set(void)
{
    AWGDevice *device = awg_device_new();
    g_assert_nonnull(device);

    awg_device_set_address_from_string(device, "10.0.0.2/32");
    awg_device_set_private_key(device, "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20=");
    awg_device_set_mtu(device, 1500);

    gchar *config_str = awg_device_create_config_string(device);
    g_assert_nonnull(config_str);

    g_assert_nonnull(strstr(config_str, "MTU = 1500"));

    g_free(config_str);
    g_object_unref(device);
}

static void
test_awg_device_mtu_validation(void)
{
    AWGDevice *device = awg_device_new();
    g_assert_nonnull(device);

    g_assert_true(awg_device_set_mtu(device, 0));
    g_assert_cmpint(awg_device_get_mtu(device), ==, 0);

    g_assert_true(awg_device_set_mtu(device, 68));
    g_assert_cmpint(awg_device_get_mtu(device), ==, 68);

    g_assert_true(awg_device_set_mtu(device, 1420));
    g_assert_cmpint(awg_device_get_mtu(device), ==, 1420);

    g_assert_true(awg_device_set_mtu(device, 1500));
    g_assert_cmpint(awg_device_get_mtu(device), ==, 1500);

    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*MTU value * is out of valid range*");
    g_assert_false(awg_device_set_mtu(device, 67));
    g_assert_cmpint(awg_device_get_mtu(device), ==, 1500);
    g_test_assert_expected_messages();

    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*MTU value * is out of valid range*");
    g_assert_false(awg_device_set_mtu(device, 1501));
    g_assert_cmpint(awg_device_get_mtu(device), ==, 1500);
    g_test_assert_expected_messages();

    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*MTU value * is out of valid range*");
    g_assert_false(awg_device_set_mtu(device, 65535));
    g_assert_cmpint(awg_device_get_mtu(device), ==, 1500);
    g_test_assert_expected_messages();

    g_object_unref(device);
}

static void
test_awg_device_is_valid(void)
{
    gchar *config_path = get_test_config_path("test-config-ipv4.conf");
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);
    g_assert_nonnull(device);
    g_assert_true(awg_device_is_valid(device));

    g_object_unref(device);
    g_free(config_path);
}

static void
test_awg_subnet_ipv4(void)
{
    AWGSubNet *subnet = awg_subnet_new_from_string("10.0.0.1/24");
    g_assert_nonnull(subnet);

    gchar *str = awg_subnet_to_string(subnet);
    g_assert_cmpstr(str, ==, "10.0.0.1/24");
    g_free(str);

    g_object_unref(subnet);
}

static void
test_awg_subnet_ipv6(void)
{
    AWGSubNet *subnet = awg_subnet_new_from_string("fd00::1/64");
    g_assert_nonnull(subnet);

    gchar *str = awg_subnet_to_string(subnet);
    g_assert_cmpstr(str, ==, "fd00::1/64");
    g_free(str);

    g_object_unref(subnet);
}

static void
test_awg_subnet_ipv6_brackets(void)
{
    AWGSubNet *subnet = awg_subnet_new_from_string("[2001:db8::1]/128");
    g_assert_nonnull(subnet);

    gchar *str = awg_subnet_to_string(subnet);
    g_assert_cmpstr(str, ==, "2001:db8::1/128");
    g_free(str);

    g_object_unref(subnet);

    g_assert_null(awg_subnet_new_from_string("[2001:db8::1]garbage/64"));
    g_assert_null(awg_subnet_new_from_string("[2001:db8::1]garbage"));
}

static void
test_awg_endpoint_ipv4(void)
{
    AWGDevicePeer *peer = awg_device_peer_new();
    g_assert_true(awg_device_peer_set_endpoint(peer, "192.168.1.1:51820"));
    g_assert_cmpstr(awg_device_peer_get_endpoint(peer), ==, "192.168.1.1:51820");
    g_object_unref(peer);
}

static void
test_awg_endpoint_ipv6(void)
{
    AWGDevicePeer *peer = awg_device_peer_new();
    g_assert_true(awg_device_peer_set_endpoint(peer, "[2001:db8::1]:51820"));
    g_assert_cmpstr(awg_device_peer_get_endpoint(peer), ==, "[2001:db8::1]:51820");
    g_object_unref(peer);
}

static void
test_awg_endpoint_hostname(void)
{
    AWGDevicePeer *peer = awg_device_peer_new();
    g_assert_true(awg_device_peer_set_endpoint(peer, "vpn.example.com:51820"));
    g_assert_cmpstr(awg_device_peer_get_endpoint(peer), ==, "vpn.example.com:51820");
    g_object_unref(peer);
}

static void
test_awg_endpoint_invalid(void)
{
    AWGDevicePeer *peer = awg_device_peer_new();
    g_assert_false(awg_device_peer_set_endpoint(peer, "invalid"));
    g_assert_false(awg_device_peer_set_endpoint(peer, ":51820"));
    g_assert_false(awg_device_peer_set_endpoint(peer, "host:"));
    g_assert_false(awg_device_peer_set_endpoint(peer, "host:99999"));
    g_object_unref(peer);
}

static void
test_awg_validate_empty(void)
{
    g_assert_true(awg_validate_empty(NULL));
    g_assert_true(awg_validate_empty(""));
    g_assert_true(awg_validate_empty("   "));
    g_assert_false(awg_validate_empty("value"));
}

static void
test_awg_validate_ip4_valid(void)
{
    g_assert_true(awg_validate_ip4("10.0.0.1"));
    g_assert_true(awg_validate_ip4("192.168.1.1"));
    g_assert_true(awg_validate_ip4("255.255.255.255"));
    g_assert_true(awg_validate_ip4("0.0.0.0"));
}

static void
test_awg_validate_ip4_invalid(void)
{
    g_assert_false(awg_validate_ip4(NULL));
    g_assert_false(awg_validate_ip4(""));
    g_assert_false(awg_validate_ip4("256.0.0.1"));
    g_assert_false(awg_validate_ip4("10.0.0"));
    g_assert_false(awg_validate_ip4("invalid"));
}

static void
test_awg_validate_ip6_valid(void)
{
    g_assert_true(awg_validate_ip6("::1"));
    g_assert_true(awg_validate_ip6("2001:db8::1"));
    g_assert_true(awg_validate_ip6("fe80::1"));
    g_assert_true(awg_validate_ip6("::"));
}

static void
test_awg_validate_ip6_invalid(void)
{
    g_assert_false(awg_validate_ip6(NULL));
    g_assert_false(awg_validate_ip6(""));
    g_assert_false(awg_validate_ip6("not-ipv6"));
}

static void
test_awg_validate_port_valid(void)
{
    g_assert_true(awg_validate_port(""));
    g_assert_true(awg_validate_port("0"));
    g_assert_true(awg_validate_port("51820"));
    g_assert_true(awg_validate_port("65535"));
    g_assert_false(awg_validate_port("65536"));
}

static void
test_awg_validate_mtu_valid(void)
{
    g_assert_true(awg_validate_mtu(""));
    g_assert_true(awg_validate_mtu("68"));
    g_assert_true(awg_validate_mtu("1500"));
    g_assert_true(awg_validate_mtu("1420"));
    g_assert_false(awg_validate_mtu("67"));
    g_assert_false(awg_validate_mtu("1501"));
}

static void
test_awg_validate_jc_valid(void)
{
    g_assert_true(awg_validate_jc(""));
    g_assert_true(awg_validate_jc("0"));
    g_assert_true(awg_validate_jc("1"));
    g_assert_true(awg_validate_jc("65535"));
}

static void
test_awg_validate_jmin_valid(void)
{
    g_assert_true(awg_validate_jmin(""));
    g_assert_true(awg_validate_jmin("0"));
    g_assert_true(awg_validate_jmin("1"));
    g_assert_true(awg_validate_jmin("65535"));
}

static void
test_awg_validate_jmax_valid(void)
{
    g_assert_true(awg_validate_jmax(""));
    g_assert_true(awg_validate_jmax("0"));
    g_assert_true(awg_validate_jmax("1"));
    g_assert_true(awg_validate_jmax("65534"));
    g_assert_false(awg_validate_jmax("65535"));
}

static void
test_awg_validate_base64_valid(void)
{
    g_assert_true(awg_validate_base64("YWFhYmFhYmFhYmFhYmFhYmFhYmFhYmFhYmFhYmE="));
    g_assert_false(awg_validate_base64(NULL));
}

static void
test_awg_validate_fqdn_valid(void)
{
    g_assert_true(awg_validate_fqdn("example.com"));
    g_assert_true(awg_validate_fqdn("sub.example.com"));
    g_assert_true(awg_validate_fqdn("my-vpn-server.com"));
    g_assert_false(awg_validate_fqdn(""));
    g_assert_false(awg_validate_fqdn("10.0.0.1"));
}

static void
test_awg_validate_subnet_valid(void)
{
    g_assert_true(awg_validate_subnet("10.0.0.0/24"));
    g_assert_true(awg_validate_subnet("192.168.0.0/16"));
    g_assert_true(awg_validate_subnet("0.0.0.0/0"));
    g_assert_true(awg_validate_subnet("10.0.0.1/32"));
    g_assert_false(awg_validate_subnet("10.0.0.1"));
    g_assert_false(awg_validate_subnet("10.0.0.0/33"));
}

static void
test_awg_validate_allowed_ips_valid(void)
{
    g_assert_true(awg_validate_allowed_ips(""));
    g_assert_true(awg_validate_allowed_ips("10.0.0.0/24"));
    g_assert_true(awg_validate_allowed_ips("10.0.0.0/24, 192.168.0.0/16"));
    g_assert_true(awg_validate_allowed_ips("0.0.0.0/0"));
    g_assert_false(awg_validate_allowed_ips("10.0.0.1"));
}

static void
test_awg_validate_endpoint_valid(void)
{
    g_assert_true(awg_validate_endpoint(""));
    g_assert_true(awg_validate_endpoint("vpn.example.com:51820"));
    g_assert_true(awg_validate_endpoint("192.168.1.1:51820"));
    g_assert_true(awg_validate_endpoint("[::1]:51820"));
    g_assert_false(awg_validate_endpoint("vpn.example.com"));
    g_assert_false(awg_validate_endpoint("vpn.example.com:"));
    g_assert_false(awg_validate_endpoint("vpn.example.com:99999"));
}

static void
test_awg_endpoint_ipv6_empty_brackets(void)
{
    AWGDevicePeer *peer = awg_device_peer_new();
    g_assert_false(awg_device_peer_set_endpoint(peer, "[::]"));
    g_assert_false(awg_device_peer_set_endpoint(peer, "[::]:"));
    g_object_unref(peer);
}

static void
test_awg_config_string_no_dns_leak(void)
{
    AWGDevice *device = awg_device_new();
    g_assert_nonnull(device);

    awg_device_set_address_from_string(device, "10.0.0.2/32");
    awg_device_set_private_key(device, "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20=");
    awg_device_set_jc(device, 3);
    awg_device_set_jmin(device, 15);
    awg_device_set_jmax(device, 30);
    awg_device_set_s1(device, 10);
    awg_device_set_s2(device, 20);
    awg_device_set_h1(device, "1");
    awg_device_set_h2(device, "2");
    awg_device_set_h3(device, "3");
    awg_device_set_h4(device, "4");

    AWGDevicePeer *peer = awg_device_peer_new();
    g_assert_true(awg_device_peer_set_public_key(peer, "GevpLmD6PchV4uC2vS0n/1kLJZ5rK9jE3nN2mM8XzY0="));
    g_assert_true(awg_device_peer_set_endpoint(peer, "10.0.0.1:51820"));
    g_assert_true(awg_device_peer_set_allowed_ips_from_string(peer, "0.0.0.0/0"));
    g_assert_true(awg_device_add_peer(device, peer));
    g_object_unref(peer);

    gchar *config_str = awg_device_create_config_string(device);
    g_assert_nonnull(config_str);
    g_assert_null(strstr(config_str, "DNS"));
    g_free(config_str);

    g_object_unref(device);
}

static void
test_awg_config_string_with_dns(void)
{
    AWGDevice *device = awg_device_new();
    g_assert_nonnull(device);

    awg_device_set_address_from_string(device, "10.0.0.2/32");
    awg_device_set_private_key(device, "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20=");
    awg_device_set_dns_from_string(device, "8.8.8.8, 8.8.4.4");
    awg_device_set_jc(device, 3);
    awg_device_set_jmin(device, 15);
    awg_device_set_jmax(device, 30);
    awg_device_set_s1(device, 10);
    awg_device_set_s2(device, 20);
    awg_device_set_h1(device, "1");
    awg_device_set_h2(device, "2");
    awg_device_set_h3(device, "3");
    awg_device_set_h4(device, "4");

    AWGDevicePeer *peer = awg_device_peer_new();
    g_assert_true(awg_device_peer_set_public_key(peer, "GevpLmD6PchV4uC2vS0n/1kLJZ5rK9jE3nN2mM8XzY0="));
    g_assert_true(awg_device_peer_set_endpoint(peer, "10.0.0.1:51820"));
    g_assert_true(awg_device_peer_set_allowed_ips_from_string(peer, "0.0.0.0/0"));
    g_assert_true(awg_device_add_peer(device, peer));
    g_object_unref(peer);

    gchar *config_str = awg_device_create_config_string(device);
    g_assert_nonnull(config_str);
    g_assert_nonnull(strstr(config_str, "DNS = 8.8.8.8, 8.8.4.4"));
    g_free(config_str);

    g_object_unref(device);
}

static void
test_awg_device_new_from_config_extended(void)
{
    gchar *config_path = get_test_config_path("test-config-extended.conf");
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);

    g_assert_nonnull(device);

    g_assert_cmpint(awg_device_get_jc(device), ==, 3);
    g_assert_cmpint(awg_device_get_jmin(device), ==, 64);
    g_assert_cmpint(awg_device_get_jmax(device), ==, 128);
    g_assert_cmpint(awg_device_get_s1(device), ==, 10);
    g_assert_cmpint(awg_device_get_s2(device), ==, 20);
    g_assert_cmpint(awg_device_get_s3(device), ==, 30);
    g_assert_cmpint(awg_device_get_s4(device), ==, 40);

    g_assert_cmpstr(awg_device_get_h1(device), ==, "1-100");
    g_assert_cmpstr(awg_device_get_h2(device), ==, "101-200");
    g_assert_cmpstr(awg_device_get_h3(device), ==, "201-300");
    g_assert_cmpstr(awg_device_get_h4(device), ==, "301-400");

    g_assert_cmpstr(awg_device_get_i1(device), ==, "<r 16>");
    g_assert_cmpstr(awg_device_get_i2(device), ==, "<rc 10>");
    g_assert_cmpstr(awg_device_get_i3(device), ==, "<b 0xDEADBEEF>");
    g_assert_cmpstr(awg_device_get_i4(device), ==, "<r 16><t>");
    g_assert_cmpstr(awg_device_get_i5(device), ==, "<b 0xAABBCCDD><rc 5><rd 3><c>");

    g_object_unref(device);
    g_free(config_path);
}

static void
test_awg_s3_s4_setters(void)
{
    AWGDevice *device = awg_device_new();
    g_assert_nonnull(device);

    g_assert_cmpint(awg_device_get_s3(device), ==, 0);
    g_assert_cmpint(awg_device_get_s4(device), ==, 0);

    g_assert_true(awg_device_set_s3(device, 100));
    g_assert_cmpint(awg_device_get_s3(device), ==, 100);

    g_assert_true(awg_device_set_s4(device, 200));
    g_assert_cmpint(awg_device_get_s4(device), ==, 200);

    g_assert_true(awg_device_set_s3(device, 0));
    g_assert_cmpint(awg_device_get_s3(device), ==, 0);

    g_object_unref(device);
}

static void
test_awg_i1_i5_setters(void)
{
    AWGDevice *device = awg_device_new();
    g_assert_nonnull(device);

    g_assert_null(awg_device_get_i1(device));
    g_assert_null(awg_device_get_i5(device));

    g_assert_true(awg_device_set_i1(device, "<r 16>"));
    g_assert_cmpstr(awg_device_get_i1(device), ==, "<r 16>");

    g_assert_true(awg_device_set_i2(device, "<rc 10>"));
    g_assert_cmpstr(awg_device_get_i2(device), ==, "<rc 10>");

    g_assert_true(awg_device_set_i3(device, "<b 0xDEADBEEF>"));
    g_assert_cmpstr(awg_device_get_i3(device), ==, "<b 0xDEADBEEF>");

    g_assert_true(awg_device_set_i4(device, "<r 16><t>"));
    g_assert_cmpstr(awg_device_get_i4(device), ==, "<r 16><t>");

    g_assert_true(awg_device_set_i5(device, "<c>"));
    g_assert_cmpstr(awg_device_get_i5(device), ==, "<c>");

    g_assert_true(awg_device_set_i1(device, NULL));
    g_assert_null(awg_device_get_i1(device));

    g_assert_true(awg_device_set_i1(device, ""));
    g_assert_null(awg_device_get_i1(device));

    g_object_unref(device);
}

static void
test_awg_h1_h4_string_values(void)
{
    AWGDevice *device = awg_device_new();
    g_assert_nonnull(device);

    g_assert_true(awg_device_set_h1(device, "1-100"));
    g_assert_cmpstr(awg_device_get_h1(device), ==, "1-100");

    g_assert_true(awg_device_set_h2(device, "200"));
    g_assert_cmpstr(awg_device_get_h2(device), ==, "200");

    g_assert_true(awg_device_set_h3(device, "1-50"));
    g_assert_cmpstr(awg_device_get_h3(device), ==, "1-50");

    g_assert_true(awg_device_set_h4(device, "100"));
    g_assert_cmpstr(awg_device_get_h4(device), ==, "100");

    g_assert_true(awg_device_set_h1(device, NULL));
    g_assert_null(awg_device_get_h1(device));

    g_assert_true(awg_device_set_h1(device, ""));
    g_assert_null(awg_device_get_h1(device));

    g_object_unref(device);
}

static void
test_awg_h1_h4_with_spaces(void)
{
    AWGDevice *device = awg_device_new();
    g_assert_nonnull(device);

    g_assert_true(awg_device_set_h1(device, "1-100"));
    g_assert_cmpstr(awg_device_get_h1(device), ==, "1-100");

    g_assert_true(awg_device_set_h2(device, "50"));
    g_assert_cmpstr(awg_device_get_h2(device), ==, "50");

    g_object_unref(device);
}

static void
test_awg_i1_i5_with_spaces(void)
{
    AWGDevice *device = awg_device_new();
    g_assert_nonnull(device);

    g_assert_true(awg_device_set_i1(device, "<r 16><t>"));
    g_assert_cmpstr(awg_device_get_i1(device), ==, "<r 16><t>");

    g_assert_true(awg_device_set_i5(device, "<r 16><t> "));
    g_assert_cmpstr(awg_device_get_i5(device), ==, "<r 16><t> ");

    g_object_unref(device);
}

static void
test_awg_validate_awg_string(void)
{
    g_assert_true(awg_validate_awg_string(NULL));
    g_assert_true(awg_validate_awg_string(""));
    g_assert_true(awg_validate_awg_string("short string"));
    g_assert_true(awg_validate_awg_string("a"));
    g_assert_false(awg_validate_awg_string("line one\nline two"));
    g_assert_false(awg_validate_awg_string("line one\rline two"));

    gchar *too_long_str = g_malloc(5 * 1024 + 2);
    memset(too_long_str, 'a', 5 * 1024 + 1);
    too_long_str[5 * 1024 + 1] = '\0';
    g_assert_false(awg_validate_awg_string(too_long_str));
    g_free(too_long_str);

    gchar *valid_str = g_malloc(5 * 1024);
    memset(valid_str, 'b', 5 * 1024 - 1);
    valid_str[5 * 1024 - 1] = '\0';
    g_assert_true(awg_validate_awg_string(valid_str));
    g_free(valid_str);

    gchar *exact_max = g_malloc(5 * 1024);
    memset(exact_max, 'c', 5 * 1024 - 1);
    exact_max[5 * 1024 - 1] = '\0';
    g_assert_true(awg_validate_awg_string(exact_max));
    g_free(exact_max);
}

static void
test_awg_validate_magic_header_valid(void)
{
    g_assert_true(awg_validate_magic_header(NULL));
    g_assert_true(awg_validate_magic_header(""));
    g_assert_true(awg_validate_magic_header(" "));

    g_assert_true(awg_validate_magic_header("1"));
    g_assert_true(awg_validate_magic_header("42"));
    g_assert_true(awg_validate_magic_header("100"));

    g_assert_true(awg_validate_magic_header("1-100"));
    g_assert_true(awg_validate_magic_header("100-200"));
    g_assert_true(awg_validate_magic_header("0-100"));
    g_assert_true(awg_validate_magic_header("1-1"));
}

static void
test_awg_validate_magic_header_invalid(void)
{
    g_assert_false(awg_validate_magic_header("abc"));
    g_assert_false(awg_validate_magic_header("1abc"));
    g_assert_false(awg_validate_magic_header("abc-100"));
    g_assert_false(awg_validate_magic_header("1-"));
    g_assert_false(awg_validate_magic_header("-100"));
    g_assert_false(awg_validate_magic_header("100-1"));
    g_assert_false(awg_validate_magic_header("-1"));
}

static void
test_awg_validate_i_packet_valid(void)
{
    g_assert_true(awg_validate_i_packet(NULL));
    g_assert_true(awg_validate_i_packet(""));
    g_assert_true(awg_validate_i_packet(" "));

    g_assert_true(awg_validate_i_packet("<c>"));
    g_assert_true(awg_validate_i_packet("<t>"));
    g_assert_true(awg_validate_i_packet("<b 0xDEADBEEF>"));
    g_assert_true(awg_validate_i_packet("<b 0xAABBCCDD>"));

    g_assert_true(awg_validate_i_packet("<r 16>"));
    g_assert_true(awg_validate_i_packet("<rc 10>"));
    g_assert_true(awg_validate_i_packet("<rd 8>"));

    g_assert_true(awg_validate_i_packet("<r 16><t>"));
    g_assert_true(awg_validate_i_packet("<b 0xDEADBEEF><r 16>"));
    g_assert_true(awg_validate_i_packet("<rc 10><rd 5>"));

    g_assert_true(awg_validate_i_packet("<r 16> "));
    g_assert_true(awg_validate_i_packet(" <r 16> "));

    g_assert_true(awg_validate_i_packet("<r 65535>"));
    g_assert_true(awg_validate_i_packet("<rc 65535>"));
    g_assert_true(awg_validate_i_packet("<rd 65535>"));
}

static void
test_awg_validate_i_packet_invalid(void)
{
    g_assert_false(awg_validate_i_packet("invalid"));
    g_assert_false(awg_validate_i_packet("packet_content"));
    g_assert_false(awg_validate_i_packet("b0xGGGG"));
    g_assert_false(awg_validate_i_packet("bDEAD"));
    g_assert_false(awg_validate_i_packet("b 0xGGGG"));
    g_assert_false(awg_validate_i_packet("b0x1"));
    g_assert_false(awg_validate_i_packet("rc"));
    g_assert_false(awg_validate_i_packet("rd"));
    g_assert_false(awg_validate_i_packet("rcabc"));
    g_assert_false(awg_validate_i_packet("rdxyz"));
    g_assert_false(awg_validate_i_packet("r"));
    g_assert_false(awg_validate_i_packet("unknown"));

    g_assert_false(awg_validate_i_packet("<r16>"));
    g_assert_false(awg_validate_i_packet("<rc10>"));
    g_assert_false(awg_validate_i_packet("<rd8>"));
    g_assert_false(awg_validate_i_packet("<b0xDEADBEEF>"));

    g_assert_false(awg_validate_i_packet("<c 0>"));
    g_assert_false(awg_validate_i_packet("<t 0>"));

    g_assert_false(awg_validate_i_packet("r65536"));
    g_assert_false(awg_validate_i_packet("rc65536"));
    g_assert_false(awg_validate_i_packet("rd65536"));

    gchar *too_long = g_malloc(5 * 1024 + 100);
    memset(too_long, 'a', 5 * 1024 + 99);
    too_long[5 * 1024 + 99] = '\0';
    g_assert_false(awg_validate_i_packet(too_long));
    g_free(too_long);
}

static void
test_awg_validate_s3_valid(void)
{
    g_assert_true(awg_validate_s3(NULL));
    g_assert_true(awg_validate_s3(""));
    g_assert_true(awg_validate_s3("0"));
    g_assert_true(awg_validate_s3("1"));
    g_assert_true(awg_validate_s3("100"));
    g_assert_true(awg_validate_s3("65479"));

    g_assert_false(awg_validate_s3("65480"));
    g_assert_false(awg_validate_s3("65535"));
}

static void
test_awg_validate_s4_valid(void)
{
    g_assert_true(awg_validate_s4(NULL));
    g_assert_true(awg_validate_s4(""));
    g_assert_true(awg_validate_s4("0"));
    g_assert_true(awg_validate_s4("1"));
    g_assert_true(awg_validate_s4("100"));
    g_assert_true(awg_validate_s4("65503"));

    g_assert_false(awg_validate_s4("65504"));
    g_assert_false(awg_validate_s4("65535"));
}

static void
test_awg_validate_magic_headers_no_overlap(void)
{
    g_assert_true(awg_validate_magic_headers_no_overlap("", "", "", ""));
    g_assert_true(awg_validate_magic_headers_no_overlap("1", "2", "3", "4"));
    g_assert_true(awg_validate_magic_headers_no_overlap("1-10", "11-20", "21-30", "31-40"));
    g_assert_true(awg_validate_magic_headers_no_overlap("1-10", "20-30", "", ""));
    g_assert_true(awg_validate_magic_headers_no_overlap("", "11-20", "", ""));

    g_assert_false(awg_validate_magic_headers_no_overlap("1-10", "5-15", "", ""));
    g_assert_false(awg_validate_magic_headers_no_overlap("1-10", "2", "", ""));
    g_assert_false(awg_validate_magic_headers_no_overlap("1-10", "11-20", "15-25", ""));
    g_assert_false(awg_validate_magic_headers_no_overlap("1", "2", "3", "2"));
}

static void
test_awg_validate_jmin_jmax(void)
{
    g_assert_true(awg_validate_jmin_jmax(0, 0));
    g_assert_true(awg_validate_jmin_jmax(0, 1));
    g_assert_true(awg_validate_jmin_jmax(1, 10));
    g_assert_true(awg_validate_jmin_jmax(100, 100));
    g_assert_true(awg_validate_jmin_jmax(65534, 65535));

    g_assert_false(awg_validate_jmin_jmax(10, 5));
    g_assert_false(awg_validate_jmin_jmax(100, 50));
}

static void
test_awg_peer_clone_multi_allowed_ips(void)
{

    AWGDevicePeer *peer = awg_device_peer_new();
    g_assert_nonnull(peer);

    // Устанавливаем несколько allowed IPs
    g_assert_true(awg_device_peer_set_allowed_ips_from_string(peer, "10.0.0.0/24, 192.168.0.0/16, 0.0.0.0/0"));

    // Проверяем, что все подсети сохранились
    gchar *allowed_ips = awg_device_peer_get_allowed_ips_as_string(peer);
    g_assert_nonnull(allowed_ips);
    g_assert_cmpstr(allowed_ips, ==, "10.0.0.0/24, 192.168.0.0/16, 0.0.0.0/0");
    g_free(allowed_ips);

    // Клонируем peer
    AWGDevicePeer *clone = awg_device_peer_new_clone(peer);
    g_assert_nonnull(clone);

    // Проверяем, что у клона все подсети на месте
    gchar *clone_allowed_ips = awg_device_peer_get_allowed_ips_as_string(clone);
    g_assert_nonnull(clone_allowed_ips);
    g_assert_cmpstr(clone_allowed_ips, ==, "10.0.0.0/24, 192.168.0.0/16, 0.0.0.0/0");
    g_free(clone_allowed_ips);

    g_object_unref(peer);
    g_object_unref(clone);
}

static void
test_awg_config_string_with_s3_s4(void)
{
    AWGDevice *device = awg_device_new();
    g_assert_nonnull(device);

    awg_device_set_address_from_string(device, "10.0.0.2/32");
    awg_device_set_private_key(device, "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20=");
    awg_device_set_s3(device, 30);
    awg_device_set_s4(device, 40);

    AWGDevicePeer *peer = awg_device_peer_new();
    g_assert_true(awg_device_peer_set_public_key(peer, "GevpLmD6PchV4uC2vS0n/1kLJZ5rK9jE3nN2mM8XzY0="));
    g_assert_true(awg_device_peer_set_endpoint(peer, "10.0.0.1:51820"));
    g_assert_true(awg_device_peer_set_allowed_ips_from_string(peer, "0.0.0.0/0"));
    g_assert_true(awg_device_add_peer(device, peer));
    g_object_unref(peer);

    gchar *config_str = awg_device_create_config_string(device);
    g_assert_nonnull(config_str);

    g_assert_nonnull(strstr(config_str, "S3 = 30"));
    g_assert_nonnull(strstr(config_str, "S4 = 40"));

    g_free(config_str);
    g_object_unref(device);
}

/* Keys that mean nothing when 0 are left out of the generated file: S3/S4
 * are unknown to older AmneziaWG implementations, and amneziawg-tools 3.0
 * and newer reject "PersistentKeepalive = off". The keys every version
 * knows (Jc..S2) stay. */
static void
test_awg_config_string_omits_zero_values(void)
{
    g_autofree gchar *dir = g_dir_make_tmp("awg-zero-XXXXXX", NULL);
    g_autofree gchar *path = g_build_filename(dir, "awg0.conf", NULL);
    g_autofree gchar *config_str = NULL;
    g_autofree gchar *keepalive_str = NULL;
    AWGDevice *device = awg_device_new();
    AWGDevicePeer *peer = awg_device_peer_new();
    AWGDevice *reloaded;
    GError *error = NULL;

    g_assert_true(awg_device_set_private_key(device, "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20="));
    g_assert_true(awg_device_peer_set_public_key(peer, "GevpLmD6PchV4uC2vS0n/1kLJZ5rK9jE3nN2mM8XzY0="));
    g_assert_true(awg_device_peer_set_endpoint(peer, "10.0.0.1:51820"));
    g_assert_true(awg_device_peer_set_allowed_ips_from_string(peer, "0.0.0.0/0"));
    g_assert_true(awg_device_add_peer(device, peer));

    config_str = awg_device_create_config_string(device);
    g_assert_null(strstr(config_str, "S3"));
    g_assert_null(strstr(config_str, "S4"));
    g_assert_null(strstr(config_str, "PersistentKeepalive"));
    g_assert_null(strstr(config_str, "off"));
    g_assert_nonnull(strstr(config_str, "Jc = 0\n"));
    g_assert_nonnull(strstr(config_str, "Jmin = 0\n"));
    g_assert_nonnull(strstr(config_str, "Jmax = 0\n"));
    g_assert_nonnull(strstr(config_str, "S1 = 0\n"));
    g_assert_nonnull(strstr(config_str, "S2 = 0\n"));

    awg_device_peer_set_keep_alive_interval(peer, 25);
    keepalive_str = awg_device_create_config_string(device);
    g_assert_nonnull(strstr(keepalive_str, "PersistentKeepalive = 25\n"));

    /* What is left out reads back as 0 */
    awg_device_peer_set_keep_alive_interval(peer, 0);
    g_assert_true(awg_device_save_to_file(device, path));
    reloaded = awg_device_new_from_config(path, &error);
    g_assert_no_error(error);
    g_assert_nonnull(reloaded);
    g_assert_cmpuint(awg_device_get_s3(reloaded), ==, 0);
    g_assert_cmpuint(awg_device_get_s4(reloaded), ==, 0);
    g_assert_cmpuint(awg_device_peer_get_keep_alive_interval(awg_device_get_peers_list(reloaded)->data), ==, 0);

    g_object_unref(reloaded);
    g_object_unref(peer);
    g_object_unref(device);
    unlink(path);
    rmdir(dir);
}

/* Configurations written by older versions (and by hand) still import. */
static void
test_awg_config_keepalive_off_imports(void)
{
    g_autofree gchar *dir = g_dir_make_tmp("awg-off-XXXXXX", NULL);
    g_autofree gchar *path = g_build_filename(dir, "awg0.conf", NULL);
    AWGDevice *device;
    GError *error = NULL;

    g_assert_true(g_file_set_contents(path,
                                      "[Interface]\n"
                                      "PrivateKey = IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20=\n"
                                      "S3 = 0\n"
                                      "S4 = 0\n"
                                      "\n"
                                      "[Peer]\n"
                                      "PublicKey = GevpLmD6PchV4uC2vS0n/1kLJZ5rK9jE3nN2mM8XzY0=\n"
                                      "Endpoint = 10.0.0.1:51820\n"
                                      "AllowedIPs = 0.0.0.0/0\n"
                                      "PersistentKeepalive = off\n",
                                      -1, NULL));
    device = awg_device_new_from_config(path, &error);
    g_assert_no_error(error);
    g_assert_nonnull(device);
    g_assert_cmpuint(awg_device_peer_get_keep_alive_interval(awg_device_get_peers_list(device)->data), ==, 0);

    g_object_unref(device);
    unlink(path);
    rmdir(dir);
}

static void
test_awg_config_string_with_i1_i5(void)
{
    AWGDevice *device = awg_device_new();
    g_assert_nonnull(device);

    awg_device_set_address_from_string(device, "10.0.0.2/32");
    awg_device_set_private_key(device, "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20=");
    awg_device_set_i1(device, "<r 16>");
    awg_device_set_i2(device, "<rc 10>");
    awg_device_set_i3(device, "<b 0xDEADBEEF>");
    awg_device_set_i4(device, "<r 16><t>");
    awg_device_set_i5(device, "<rc 5><t>");

    AWGDevicePeer *peer = awg_device_peer_new();
    g_assert_true(awg_device_peer_set_public_key(peer, "GevpLmD6PchV4uC2vS0n/1kLJZ5rK9jE3nN2mM8XzY0="));
    g_assert_true(awg_device_peer_set_endpoint(peer, "10.0.0.1:51820"));
    g_assert_true(awg_device_peer_set_allowed_ips_from_string(peer, "0.0.0.0/0"));
    g_assert_true(awg_device_add_peer(device, peer));
    g_object_unref(peer);

    gchar *config_str = awg_device_create_config_string(device);
    g_assert_nonnull(config_str);

    g_assert_nonnull(strstr(config_str, "I1 = <r 16>"));
    g_assert_nonnull(strstr(config_str, "I2 = <rc 10>"));
    g_assert_nonnull(strstr(config_str, "I3 = <b 0xDEADBEEF>"));
    g_assert_nonnull(strstr(config_str, "I4 = <r 16><t>"));
    g_assert_nonnull(strstr(config_str, "I5 = <rc 5><t>"));

    g_free(config_str);
    g_object_unref(device);
}

static void
test_awg_config_string_h1_h4_strings(void)
{
    AWGDevice *device = awg_device_new();
    g_assert_nonnull(device);

    awg_device_set_address_from_string(device, "10.0.0.2/32");
    awg_device_set_private_key(device, "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20=");
    awg_device_set_h1(device, "1-100");
    awg_device_set_h2(device, "200");
    awg_device_set_h3(device, "1-50");
    awg_device_set_h4(device, "100");

    AWGDevicePeer *peer = awg_device_peer_new();
    g_assert_true(awg_device_peer_set_public_key(peer, "GevpLmD6PchV4uC2vS0n/1kLJZ5rK9jE3nN2mM8XzY0="));
    g_assert_true(awg_device_peer_set_endpoint(peer, "10.0.0.1:51820"));
    g_assert_true(awg_device_peer_set_allowed_ips_from_string(peer, "0.0.0.0/0"));
    g_assert_true(awg_device_add_peer(device, peer));
    g_object_unref(peer);

    gchar *config_str = awg_device_create_config_string(device);
    g_assert_nonnull(config_str);

    g_assert_nonnull(strstr(config_str, "H1 = 1-100"));
    g_assert_nonnull(strstr(config_str, "H2 = 200"));
    g_assert_nonnull(strstr(config_str, "H3 = 1-50"));
    g_assert_nonnull(strstr(config_str, "H4 = 100"));

    g_free(config_str);
    g_object_unref(device);
}

static void
test_awg_device_save_load_extended(void)
{
    gchar *config_path = get_test_config_path("test-config-extended.conf");
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);
    g_assert_nonnull(device);

    gchar *output_path = g_build_filename(g_get_tmp_dir(), "output-extended.conf", NULL);
    gboolean saved = awg_device_save_to_file(device, output_path);
    g_assert_true(saved);

    g_object_unref(device);

    AWGDevice *device2 = awg_device_new_from_config(output_path, NULL);
    g_assert_nonnull(device2);

    g_assert_cmpint(awg_device_get_s3(device2), ==, 30);
    g_assert_cmpint(awg_device_get_s4(device2), ==, 40);

    g_assert_cmpstr(awg_device_get_h1(device2), ==, "1-100");
    g_assert_cmpstr(awg_device_get_h4(device2), ==, "301-400");

    g_assert_cmpstr(awg_device_get_i1(device2), ==, "<r 16>");
    g_assert_cmpstr(awg_device_get_i5(device2), ==, "<b 0xAABBCCDD><rc 5><rd 3><c>");

    g_object_unref(device2);

    unlink(output_path);
    g_free(output_path);
    g_free(config_path);
}

/* Regression tests for https://github.com/.../issues (Arch Linux activation
 * failure): a connection without PrivateKey/PresharedKey in vpn.secrets
 * produces a keyless config that "awg setconf" rejects with
 * "Configuration parsing error", and NM_FORCE_AWG_QUICK must select the
 * external (awg-quick) manager. */

/* Импортированный профиль должен маршрутизироваться в NetworkManager:
 *  - ipv4.method = auto (при manual сервис плагина не ставит маршруты из AllowedIPs);
 *  - never-default = yes, если профиль не маршрутизирует всё (список адресов),
 *    и no для полного туннеля (0.0.0.0/0) — иначе NM заворачивает в туннель
 *    и те адреса, которые должны идти мимо него.
 */
static void
test_nm_connection_never_default(void)
{
    GError *error = NULL;

    /* Список адресов: маршрут по умолчанию через VPN не нужен. */
    {
        gchar *config_path = get_test_config_path("test-config-split-tunnel.conf");
        AWGDevice *device = awg_device_new_from_config(config_path, NULL);
        NMConnection *connection = nm_simple_connection_new();
        NMSettingIPConfig *s_ip4;

        g_assert_nonnull(device);
        g_assert_true(awg_device_save_to_nm_connection(device, connection, &error));
        g_assert_no_error(error);

        s_ip4 = nm_connection_get_setting_ip4_config(connection);
        g_assert_nonnull(s_ip4);
        g_assert_cmpstr(nm_setting_ip_config_get_method(s_ip4), ==, NM_SETTING_IP4_CONFIG_METHOD_AUTO);
        g_assert_true(nm_setting_ip_config_get_never_default(s_ip4));

        g_object_unref(connection);
        g_object_unref(device);
        g_free(config_path);
    }

    /* Полный туннель: VPN и есть маршрут по умолчанию. */
    {
        gchar *config_path = get_test_config_path("test-config-ipv4.conf");
        AWGDevice *device = awg_device_new_from_config(config_path, NULL);
        NMConnection *connection = nm_simple_connection_new();
        NMSettingIPConfig *s_ip4;

        g_assert_nonnull(device);
        g_assert_true(awg_device_save_to_nm_connection(device, connection, &error));
        g_assert_no_error(error);

        s_ip4 = nm_connection_get_setting_ip4_config(connection);
        g_assert_nonnull(s_ip4);
        g_assert_cmpstr(nm_setting_ip_config_get_method(s_ip4), ==, NM_SETTING_IP4_CONFIG_METHOD_AUTO);
        g_assert_false(nm_setting_ip_config_get_never_default(s_ip4));

        g_object_unref(connection);
        g_object_unref(device);
        g_free(config_path);
    }
}

static void
test_nm_connection_secrets_roundtrip(void)
{
    gchar *config_path = get_test_config_path("test-config-ipv4.conf");
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);
    g_assert_nonnull(device);
    g_assert_cmpstr(awg_device_get_private_key(device), ==, "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20=");

    const GList *peers = awg_device_get_peers_list(device);
    g_assert_nonnull(peers);
    g_assert_cmpstr(awg_device_peer_get_shared_key(peers->data), ==, "2hAJ4eqUN13Ue6DjcLn3MGq6ARllKvI6lzg6Uh62K+w=");

    /* config -> NMConnection (what "nmcli connection import" stores) */
    NMConnection *connection = nm_simple_connection_new();
    GError *error = NULL;
    g_assert_true(awg_device_save_to_nm_connection(device, connection, &error));
    g_assert_no_error(error);

    NMSettingVpn *s_vpn = nm_connection_get_setting_vpn(connection);
    g_assert_nonnull(s_vpn);
    g_assert_cmpstr(nm_setting_vpn_get_secret(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY), ==,
                    "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20=");

    {
        NMSettingSecretFlags flags = NM_SETTING_SECRET_FLAG_NOT_SAVED;
        g_assert_true(nm_setting_get_secret_flags(NM_SETTING(s_vpn), NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY, &flags, NULL));
        g_assert_cmpint(flags, ==, NM_SETTING_SECRET_FLAG_NONE);
    }

    {
        g_autofree gchar *psk_key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PRESHARED_KEY, 0);
        g_assert_cmpstr(nm_setting_vpn_get_secret(s_vpn, psk_key), ==, "2hAJ4eqUN13Ue6DjcLn3MGq6ARllKvI6lzg6Uh62K+w=");

        NMSettingSecretFlags flags = NM_SETTING_SECRET_FLAG_NOT_SAVED;
        g_assert_true(nm_setting_get_secret_flags(NM_SETTING(s_vpn), psk_key, &flags, NULL));
        g_assert_cmpint(flags, ==, NM_SETTING_SECRET_FLAG_NONE);

        g_autofree gchar *double_flags_key = g_strdup_printf("%s-flags-flags", psk_key);
        g_assert_null(nm_setting_vpn_get_data_item(s_vpn, double_flags_key));
    }

    /* NMConnection -> AWGDevice (what the service builds on activation) */
    AWGDevice *device2 = awg_device_new_from_nm_connection(connection, &error);
    g_assert_no_error(error);
    g_assert_nonnull(device2);
    g_assert_true(awg_device_is_valid(device2));
    g_assert_cmpstr(awg_device_get_private_key(device2), ==, awg_device_get_private_key(device));

    const GList *peers2 = awg_device_get_peers_list(device2);
    g_assert_nonnull(peers2);
    g_assert_cmpstr(awg_device_peer_get_shared_key(peers2->data), ==,
                    awg_device_peer_get_shared_key(peers->data));

    /* The generated awg-quick config must contain both keys */
    g_autofree gchar *config_str = awg_device_create_config_string(device2);
    g_assert_nonnull(config_str);
    g_assert_nonnull(strstr(config_str, "PrivateKey = IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20="));
    g_assert_nonnull(strstr(config_str, "PresharedKey = 2hAJ4eqUN13Ue6DjcLn3MGq6ARllKvI6lzg6Uh62K+w="));

    g_object_unref(device2);
    g_object_unref(connection);
    g_object_unref(device);
    g_free(config_path);
}

static void
test_keyless_config_detected(void)
{
    /* A config without PrivateKey/PresharedKey is rejected at import time */
    gchar *config_path = get_test_config_path("test-config-no-keys.conf");
    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*Invalid AWG device configuration*");
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);
    g_assert_null(device);
    g_test_assert_expected_messages();
    g_free(config_path);

    /* ... but the same keyless state can still reach the service through
     * NMConnection restore (lost vpn.secrets), which performs no validation.
     * That keyless device is what produced a "Configuration parsing error"
     * from "awg setconf" on activation. */
    AWGDevice *keyless = awg_device_new();
    AWGDevicePeer *peer = awg_device_peer_new();
    g_assert_true(awg_device_peer_set_public_key(peer, "9rLL/fiLgF39EZnzj1xSwIHrY3G+AIwUtnfDpR2H8uU="));
    g_assert_true(awg_device_peer_set_endpoint(peer, "192.168.1.1:51820"));
    g_assert_true(awg_device_peer_set_allowed_ips_from_string(peer, "0.0.0.0/0"));
    g_assert_true(awg_device_add_peer(keyless, peer));
    g_object_unref(peer);

    NMConnection *connection = nm_simple_connection_new();
    GError *error = NULL;
    g_assert_true(awg_device_save_to_nm_connection(keyless, connection, &error));
    g_assert_no_error(error);

    AWGDevice *restored = awg_device_new_from_nm_connection(connection, &error);
    g_assert_no_error(error);
    g_assert_nonnull(restored);
    g_assert_false(awg_device_is_valid(restored));

    g_autofree gchar *reason = awg_device_get_invalid_reason(restored);
    g_assert_nonnull(reason);
    g_assert_nonnull(strstr(reason, "PrivateKey"));

    g_autofree gchar *config_str = awg_device_create_config_string(restored);
    g_assert_nonnull(config_str);
    g_assert_null(strstr(config_str, "PrivateKey"));

    g_object_unref(restored);
    g_object_unref(connection);
    g_object_unref(keyless);
}

static void
test_invalid_reason_messages(void)
{
    /* Valid device -> no reason */
    gchar *config_path = get_test_config_path("test-config-ipv4.conf");
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);
    g_assert_nonnull(device);
    g_assert_null(awg_device_get_invalid_reason(device));

    /* No peers */
    {
        AWGDevice *empty = awg_device_new();
        awg_device_set_private_key(empty, "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20=");
        g_assert_false(awg_device_is_valid(empty));
        g_autofree gchar *reason = awg_device_get_invalid_reason(empty);
        g_assert_nonnull(reason);
        g_assert_nonnull(strstr(reason, "peer"));
        g_object_unref(empty);
    }

    /* Peer without public key (injected via replace_peer, which unlike
     * add_peer performs no validation; NMConnection restore and .conf import
     * go through add_peer, which drops such a peer) */
    {
        gchar *peer_config_path = get_test_config_path("test-config-ipv4.conf");
        AWGDevice *d = awg_device_new_from_config(peer_config_path, NULL);
        g_assert_nonnull(d);
        g_free(peer_config_path);
        AWGDevicePeer *bad = awg_device_peer_new();
        g_assert_true(awg_device_peer_set_endpoint(bad, "192.168.1.1:51820"));
        g_assert_true(awg_device_peer_set_allowed_ips_from_string(bad, "0.0.0.0/0"));
        g_assert_true(awg_device_replace_peer(d, 0, bad));
        g_object_unref(bad);
        g_assert_false(awg_device_is_valid(d));
        g_autofree gchar *reason = awg_device_get_invalid_reason(d);
        g_assert_nonnull(reason);
        g_assert_nonnull(strstr(reason, "Peer 0: PublicKey is missing"));
        g_object_unref(d);
    }

    /* Overlapping H1-H4 ranges */
    {
        AWGDevice *d = awg_device_new();
        awg_device_set_private_key(d, "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20=");
        awg_device_set_h1(d, "1-100");
        awg_device_set_h2(d, "50-150");
        AWGDevicePeer *peer = awg_device_peer_new();
        g_assert_true(awg_device_peer_set_public_key(peer, "9rLL/fiLgF39EZnzj1xSwIHrY3G+AIwUtnfDpR2H8uU="));
        g_assert_true(awg_device_peer_set_endpoint(peer, "192.168.1.1:51820"));
        g_assert_true(awg_device_peer_set_allowed_ips_from_string(peer, "0.0.0.0/0"));
        g_assert_true(awg_device_add_peer(d, peer));
        g_object_unref(peer);
        g_assert_false(awg_device_is_valid(d));
        g_autofree gchar *reason = awg_device_get_invalid_reason(d);
        g_assert_nonnull(reason);
        g_assert_nonnull(strstr(reason, "overlap"));
        g_object_unref(d);
    }

    g_object_unref(device);
    g_free(config_path);
}

static void
test_force_awg_quick_selects_external(void)
{
    /* Hermetic stub for awg-quick so the test does not depend on the host */
    g_autofree gchar *stub_path = g_build_filename(g_get_tmp_dir(), "test-awg-quick-stub", NULL);
    g_assert_true(g_file_set_contents(stub_path, "#!/bin/sh\nexit 0\n", -1, NULL));
    g_assert_cmpint(chmod(stub_path, 0755), ==, 0);

    const gchar *old_quick_path = g_getenv("NM_AWG_QUICK_PATH");
    const gchar *old_force = g_getenv("NM_FORCE_AWG_QUICK");
    g_autofree gchar *saved_quick_path = old_quick_path ? g_strdup(old_quick_path) : NULL;
    g_autofree gchar *saved_force = old_force ? g_strdup(old_force) : NULL;

    g_setenv("NM_AWG_QUICK_PATH", stub_path, TRUE);
    g_assert_true(awg_connection_manager_external_is_available());

    g_setenv("NM_FORCE_AWG_QUICK", "1", TRUE);

    AWGDevice *device = awg_device_new();
    g_assert_nonnull(device);
    AWGConnectionManager *mgr = awg_connection_manager_auto_new("testawg0", device);
    g_assert_nonnull(mgr);
    /* Must be the external manager even if the netlink module is present */
    g_assert_true(AWG_IS_CONNECTION_MANAGER_EXTERNAL(mgr));
    g_assert_true(awg_connection_manager_manages_routes(mgr));
    /* Outside NetworkManager's private /tmp, see the next test */
    {
        g_autofree gchar *config_dir = NULL;

        g_autofree gchar *module_version_file = NULL;

        g_object_get(mgr, "config-dir", &config_dir, "module-version-file", &module_version_file, NULL);
        g_assert_cmpstr(config_dir, ==, "/run/nm-amneziawg");
        g_assert_cmpstr(module_version_file, ==, "/sys/module/amneziawg/version");
    }

    g_object_unref(mgr);
    g_object_unref(device);

    if (saved_quick_path)
        g_setenv("NM_AWG_QUICK_PATH", saved_quick_path, TRUE);
    else
        g_unsetenv("NM_AWG_QUICK_PATH");
    if (saved_force)
        g_setenv("NM_FORCE_AWG_QUICK", saved_force, TRUE);
    else
        g_unsetenv("NM_FORCE_AWG_QUICK");
    unlink(stub_path);
}

static void
test_peer_shared_key_flags_roundtrip(void)
{
    AWGDevice *device = awg_device_new();
    AWGDevicePeer *peer = awg_device_peer_new();
    AWGDevice *restored;
    NMConnection *connection = nm_simple_connection_new();
    NMSettingVpn *s_vpn;
    GError *error = NULL;

    g_assert_true(awg_device_set_private_key(device, "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20="));
    g_assert_true(awg_device_peer_set_public_key(peer, "9rLL/fiLgF39EZnzj1xSwIHrY3G+AIwUtnfDpR2H8uU="));
    g_assert_true(awg_device_peer_set_endpoint(peer, "192.168.1.1:51820"));
    g_assert_true(awg_device_peer_set_allowed_ips_from_string(peer, "0.0.0.0/0"));
    g_assert_true(awg_device_peer_set_shared_key(peer, "2hAJ4eqUN13Ue6DjcLn3MGq6ARllKvI6lzg6Uh62K+w="));
    awg_device_peer_set_shared_key_flags(peer, NM_SETTING_SECRET_FLAG_NOT_REQUIRED);
    g_assert_true(awg_device_add_peer(device, peer));
    g_object_unref(peer);

    g_assert_true(awg_device_save_to_nm_connection(device, connection, &error));
    g_assert_no_error(error);
    s_vpn = nm_connection_get_setting_vpn(connection);
    g_assert_nonnull(s_vpn);

    /* libnm persists the flags of a secret as "<secret-name>-flags". */
    g_assert_cmpstr(nm_setting_vpn_get_data_item(s_vpn, "peer-0-preshared-key-flags"), ==, "4");
    g_assert_null(nm_setting_vpn_get_data_item(s_vpn, "peer-0-preshared-key-flags-flags"));

    restored = awg_device_new_from_nm_connection(connection, &error);
    g_assert_no_error(error);
    g_assert_nonnull(restored);
    peer = AWG_DEVICE_PEER(awg_device_get_peers_list(restored)->data);
    g_assert_nonnull(peer);
    g_assert_cmpint(awg_device_peer_get_shared_key_flags(peer), ==, NM_SETTING_SECRET_FLAG_NOT_REQUIRED);

    g_object_unref(restored);
    g_object_unref(connection);
    g_object_unref(device);
}

static void
test_awg_range_parse_u32(void)
{
    guint32 packed;

    /* Packed as hi<<16 | lo, matching the kernel's u16_range_init(). */
    g_assert_true(awg_range_parse_u32("2-10", &packed));
    g_assert_cmpuint(packed, ==, (10u << 16) | 2u);

    g_assert_true(awg_range_parse_u32("115-125", &packed));
    g_assert_cmpuint(packed, ==, (125u << 16) | 115u);

    /* A bare number is the degenerate range [n, n]. */
    g_assert_true(awg_range_parse_u32("24", &packed));
    g_assert_cmpuint(packed, ==, (24u << 16) | 24u);

    g_assert_true(awg_range_parse_u32("65535", &packed));
    g_assert_cmpuint(packed, ==, (65535u << 16) | 65535u);

    /* Rejected: above u16, inverted, malformed, empty. */
    g_assert_false(awg_range_parse_u32("65536", &packed));
    g_assert_false(awg_range_parse_u32("2-70000", &packed));
    g_assert_false(awg_range_parse_u32("10-2", &packed));
    g_assert_false(awg_range_parse_u32("abc", &packed));
    g_assert_false(awg_range_parse_u32("-5", &packed));
    g_assert_false(awg_range_parse_u32("", &packed));
    g_assert_false(awg_range_parse_u32(NULL, &packed));
}

static void
test_awg_device_header_protection_key(void)
{
    AWGDevice *device = awg_device_new();

    /* The kernel requires exactly 32 bytes (NLA_POLICY_EXACT_LEN), so a
     * well-formed base64 string of the wrong length must be rejected rather
     * than silently dropped later on.
     */
    g_assert_true(awg_device_set_header_protection_key(device, "65P9KtswVjU7zVR1f5915+lhDOEi62lMYclBlkP6304="));
    g_assert_cmpstr(awg_device_get_header_protection_key(device), ==, "65P9KtswVjU7zVR1f5915+lhDOEi62lMYclBlkP6304=");

    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Invalid HeaderProtectionKey*");
    g_assert_false(awg_device_set_header_protection_key(device, "c2hvcnQ="));
    g_test_assert_expected_messages();
    /* Rejected input must not clobber the previously accepted key. */
    g_assert_cmpstr(awg_device_get_header_protection_key(device), ==, "65P9KtswVjU7zVR1f5915+lhDOEi62lMYclBlkP6304=");

    /* Empty clears the key. */
    g_assert_true(awg_device_set_header_protection_key(device, ""));
    g_assert_null(awg_device_get_header_protection_key(device));

    g_object_unref(device);
}

static void
test_awg_device_range_setters_reject_over_u16(void)
{
    AWGDevice *device = awg_device_new();

    g_assert_true(awg_device_set_content_padding_addition(device, "2-10"));
    g_assert_cmpstr(awg_device_get_content_padding_addition(device), ==, "2-10");

    /* Values the kernel encoding cannot represent must not be accepted here,
     * otherwise they would be dropped silently on the netlink path.
     */
    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "Invalid ContentPaddingAddition*");
    g_assert_false(awg_device_set_content_padding_addition(device, "2-70000"));
    g_test_assert_expected_messages();
    g_assert_cmpstr(awg_device_get_content_padding_addition(device), ==, "2-10");

    g_object_unref(device);
}

static void
test_awg_config_awg31_roundtrip(void)
{
    gchar *config_path = get_test_config_path("test-config-awg31.conf");
    gchar *output_path = g_build_filename(g_get_tmp_dir(), "output-awg31.conf", NULL);
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);
    AWGDevice *reloaded;

    g_assert_nonnull(device);
    g_assert_cmpstr(awg_device_get_header_protection_key(device), ==, "65P9KtswVjU7zVR1f5915+lhDOEi62lMYclBlkP6304=");
    g_assert_cmpstr(awg_device_get_content_padding_addition(device), ==, "2-10");
    g_assert_cmpstr(awg_device_get_rekey_after_time(device), ==, "115-125");
    g_assert_cmpstr(awg_device_get_rekey_timeout(device), ==, "5-6");
    g_assert_cmpstr(awg_device_get_reject_after_time(device), ==, "175-185");
    g_assert_cmpstr(awg_device_get_keepalive_timeout(device), ==, "10-12");
    g_assert_cmpstr(awg_device_get_max_handshake_attempts(device), ==, "16-20");
    g_assert_true(awg_device_get_random_trailers(device));
    g_assert_true(awg_device_get_disable_cookies(device));

    /* awg-quick consumes the generated file, so the parameters must survive
     * being written back out.
     */
    g_assert_true(awg_device_save_to_file(device, output_path));
    g_object_unref(device);

    reloaded = awg_device_new_from_config(output_path, NULL);
    g_assert_nonnull(reloaded);
    g_assert_cmpstr(awg_device_get_header_protection_key(reloaded), ==, "65P9KtswVjU7zVR1f5915+lhDOEi62lMYclBlkP6304=");
    g_assert_cmpstr(awg_device_get_content_padding_addition(reloaded), ==, "2-10");
    g_assert_cmpstr(awg_device_get_rekey_after_time(reloaded), ==, "115-125");
    g_assert_cmpstr(awg_device_get_rekey_timeout(reloaded), ==, "5-6");
    g_assert_cmpstr(awg_device_get_reject_after_time(reloaded), ==, "175-185");
    g_assert_cmpstr(awg_device_get_keepalive_timeout(reloaded), ==, "10-12");
    g_assert_cmpstr(awg_device_get_max_handshake_attempts(reloaded), ==, "16-20");
    g_assert_true(awg_device_get_random_trailers(reloaded));
    g_assert_true(awg_device_get_disable_cookies(reloaded));
    g_object_unref(reloaded);

    unlink(output_path);
    g_free(output_path);
    g_free(config_path);
}

static void
test_awg_nm_connection_awg31_roundtrip(void)
{
    gchar *config_path = get_test_config_path("test-config-awg31.conf");
    NMConnection *connection = nm_simple_connection_new();
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);
    AWGDevice *restored;
    GError *error = NULL;

    g_assert_nonnull(device);
    g_assert_true(awg_device_save_to_nm_connection(device, connection, &error));
    g_assert_no_error(error);
    g_object_unref(device);

    /* The parameters travel to the service over vpn.data, so they must come
     * back out of the NMConnection unchanged.
     */
    restored = awg_device_new_from_nm_connection(connection, &error);
    g_assert_nonnull(restored);
    g_assert_no_error(error);
    g_assert_cmpstr(awg_device_get_header_protection_key(restored), ==, "65P9KtswVjU7zVR1f5915+lhDOEi62lMYclBlkP6304=");
    g_assert_cmpstr(awg_device_get_content_padding_addition(restored), ==, "2-10");
    g_assert_cmpstr(awg_device_get_rekey_after_time(restored), ==, "115-125");
    g_assert_cmpstr(awg_device_get_max_handshake_attempts(restored), ==, "16-20");
    g_assert_true(awg_device_get_random_trailers(restored));
    g_assert_true(awg_device_get_disable_cookies(restored));

    g_object_unref(restored);
    g_object_unref(connection);
    g_free(config_path);
}

static void
test_awg_version_parse(void)
{
    guint major = 0;
    guint minor = 0;

    /* Kernel module releases look like "3.1.20260812". */
    g_assert_true(awg_version_parse("3.1.20260812", &major, &minor));
    g_assert_cmpuint(major, ==, 3);
    g_assert_cmpuint(minor, ==, 1);

    g_assert_true(awg_version_parse("1.0.20260725", &major, &minor));
    g_assert_cmpuint(major, ==, 1);
    g_assert_cmpuint(minor, ==, 0);

    /* A missing or empty minor component counts as 0. */
    g_assert_true(awg_version_parse("3", &major, &minor));
    g_assert_cmpuint(major, ==, 3);
    g_assert_cmpuint(minor, ==, 0);
    g_assert_true(awg_version_parse("3.", &major, &minor));
    g_assert_cmpuint(major, ==, 3);
    g_assert_cmpuint(minor, ==, 0);

    /* Trailing noise (build suffix, URL) is ignored. */
    g_assert_true(awg_version_parse("3.1.20260812-dirty", &major, &minor));
    g_assert_cmpuint(minor, ==, 1);

    /* The tools banner is accepted as-is. */
    g_assert_true(awg_version_parse("amneziawg-tools v3.0.20260730 - https://amnezia.org", &major, &minor));
    g_assert_cmpuint(major, ==, 3);
    g_assert_cmpuint(minor, ==, 0);

    /* The minor pointer is optional. */
    g_assert_true(awg_version_parse("4.5", &major, NULL));
    g_assert_cmpuint(major, ==, 4);

    /* Rejected: no digits at all, empty, NULL, number too large for a guint. */
    g_assert_false(awg_version_parse("amneziawg", &major, &minor));
    g_assert_false(awg_version_parse("", &major, &minor));
    g_assert_false(awg_version_parse(NULL, &major, &minor));
    g_assert_false(awg_version_parse("99999999999", &major, &minor));
}

static void
test_awg_version_at_least(void)
{
    /* 3.0 is the first module/tools series with the AWG 3.1 attributes. */
    g_assert_true(awg_version_at_least("3.0.20260730", 3, 0));
    g_assert_true(awg_version_at_least("3.1.20260812", 3, 0));
    g_assert_true(awg_version_at_least("10.0", 3, 0));
    g_assert_true(awg_version_at_least("amneziawg-tools v3.0.20260730 - https://amnezia.org", 3, 0));

    /* Everything older is rejected. */
    g_assert_false(awg_version_at_least("1.0.20260725", 3, 0));
    g_assert_false(awg_version_at_least("2.0.20260101", 3, 0));

    /* Minor components are compared, not only the major one. */
    g_assert_false(awg_version_at_least("3.1.20260812", 3, 2));
    g_assert_true(awg_version_at_least("3.2", 3, 2));

    /* Unknown versions are not rejected: the backend reports the failure. */
    g_assert_true(awg_version_at_least(NULL, 3, 0));
    g_assert_true(awg_version_at_least("", 3, 0));
    g_assert_true(awg_version_at_least("unavailable", 3, 0));
}

static void
assert_required_version(AWGDevice *device, guint major, guint minor, const char *param)
{
    guint got_major = 0;
    guint got_minor = 0;
    const gchar *got_param = NULL;

    if (!param) {
        g_assert_false(awg_device_get_required_awg_version(device, &got_major, &got_minor, &got_param));
        return;
    }

    g_assert_true(awg_device_get_required_awg_version(device, &got_major, &got_minor, &got_param));
    g_assert_cmpuint(got_major, ==, major);
    g_assert_cmpuint(got_minor, ==, minor);
    g_assert_cmpstr(got_param, ==, param);
}

static void
test_awg_device_required_awg_version(void)
{
    gchar *config_path;
    AWGDevice *device;

    /* A 2.0 configuration carries none of the 3.1 parameters. */
    config_path = get_test_config_path("test-config-dual.conf");
    device = awg_device_new_from_config(config_path, NULL);
    g_assert_nonnull(device);
    assert_required_version(device, 0, 0, NULL);
    g_object_unref(device);
    g_free(config_path);

    /* A 3.1 configuration carries all of them; the flags need 3.1. */
    config_path = get_test_config_path("test-config-awg31.conf");
    device = awg_device_new_from_config(config_path, NULL);
    g_assert_nonnull(device);
    assert_required_version(device, 3, 1, "RandomTrailers");
    g_object_unref(device);
    g_free(config_path);

    device = awg_device_new();
    g_assert_nonnull(device);
    assert_required_version(device, 0, 0, NULL);

    /* Booleans count only when enabled: "off" is never encoded. */
    g_assert_true(awg_device_set_random_trailers(device, FALSE));
    g_assert_true(awg_device_set_disable_cookies(device, FALSE));
    assert_required_version(device, 0, 0, NULL);
    g_assert_true(awg_device_set_disable_cookies(device, TRUE));
    assert_required_version(device, 3, 1, "DisableCookies");
    g_assert_true(awg_device_set_disable_cookies(device, FALSE));
    assert_required_version(device, 0, 0, NULL);

    /* A single scalar parameter is enough, and needs 3.0 only ... */
    g_assert_true(awg_device_set_content_padding_addition(device, "2-10"));
    assert_required_version(device, 3, 0, "ContentPaddingAddition");

    /* ... unless a flag needs 3.1 as well. */
    g_assert_true(awg_device_set_random_trailers(device, TRUE));
    assert_required_version(device, 3, 1, "RandomTrailers");

    g_object_unref(device);
}

static AWGDevice *
external_test_device(void)
{
    AWGDevice *device = awg_device_new();
    AWGDevicePeer *peer = awg_device_peer_new();

    g_assert_true(awg_device_set_private_key(device, "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20="));
    g_assert_true(awg_device_peer_set_public_key(peer, "9rLL/fiLgF39EZnzj1xSwIHrY3G+AIwUtnfDpR2H8uU="));
    g_assert_true(awg_device_peer_set_endpoint(peer, "192.168.1.1:51820"));
    g_assert_true(awg_device_peer_set_allowed_ips_from_string(peer, "0.0.0.0/0"));
    g_assert_true(awg_device_add_peer(device, peer));
    g_object_unref(peer);

    return device;
}

/* Hermetic stubs for awg-quick (logs its arguments), awg (reports
 * @tools_version), modprobe (logs its arguments and "loads" the installed
 * module by writing its version), found through NM_AWG_QUICK_PATH and PATH,
 * and the version file of the amneziawg module (absent: no module loaded). */
typedef struct {
    gchar *dir;
    gchar *quick;
    gchar *awg;
    gchar *log;
    gchar *modprobe;
    gchar *modprobe_log;
    gchar *module_version;
    gchar *saved_path;
    gchar *saved_quick_path;
} ExternalStubs;

static AWGConnectionManager *
external_test_manager(ExternalStubs *stubs, const char *interface_name, AWGDevice *device, const char *config_dir)
{
    return g_object_new(AWG_TYPE_CONNECTION_MANAGER_EXTERNAL,
                        "interface-name", interface_name,
                        "device", device,
                        "config-dir", config_dir,
                        "module-version-file", stubs->module_version,
                        NULL);
}

/* Version of the amneziawg module modprobe loads, NULL: none installed */
static void
external_stubs_set_installed_module(ExternalStubs *stubs, const char *version)
{
    g_autofree gchar *script = NULL;

    if (version)
        script = g_strdup_printf("#!/bin/sh\necho \"$*\" >> '%s'\necho '%s' > '%s'\n",
                                 stubs->modprobe_log, version, stubs->module_version);
    else
        script = g_strdup_printf("#!/bin/sh\necho \"$*\" >> '%s'\n"
                                 "echo \"modprobe: FATAL: Module $1 not found\" >&2\nexit 1\n",
                                 stubs->modprobe_log);
    g_assert_true(g_file_set_contents(stubs->modprobe, script, -1, NULL));
    g_assert_cmpint(chmod(stubs->modprobe, 0755), ==, 0);
}

/* What modprobe was asked to load since the last call */
static gchar *
external_stubs_take_modprobe_log(ExternalStubs *stubs)
{
    gchar *contents = NULL;

    if (!g_file_get_contents(stubs->modprobe_log, &contents, NULL, NULL))
        contents = g_strdup("");
    unlink(stubs->modprobe_log);
    return contents;
}

static void
external_stubs_setup(ExternalStubs *stubs)
{
    g_autofree gchar *quick_script = NULL;
    g_autofree gchar *search_path = NULL;
    const gchar *old_path = g_getenv("PATH");

    stubs->dir = g_dir_make_tmp("awg-tools-XXXXXX", NULL);
    g_assert_nonnull(stubs->dir);
    stubs->quick = g_build_filename(stubs->dir, "awg-quick", NULL);
    stubs->awg = g_build_filename(stubs->dir, "awg", NULL);
    stubs->log = g_build_filename(stubs->dir, "awg-quick.log", NULL);
    stubs->modprobe = g_build_filename(stubs->dir, "modprobe", NULL);
    stubs->modprobe_log = g_build_filename(stubs->dir, "modprobe.log", NULL);
    stubs->module_version = g_build_filename(stubs->dir, "module-version", NULL);
    stubs->saved_path = g_strdup(old_path);
    stubs->saved_quick_path = g_strdup(g_getenv("NM_AWG_QUICK_PATH"));

    /* Logs "<command> <file> <mode of the file>" */
    quick_script = g_strdup_printf("#!/bin/sh\necho \"$1 $2 $(stat -c %%a \"$2\")\" >> '%s'\nexit 0\n", stubs->log);
    g_assert_true(g_file_set_contents(stubs->quick, quick_script, -1, NULL));
    g_assert_cmpint(chmod(stubs->quick, 0755), ==, 0);

    search_path = g_strdup_printf("%s:%s", stubs->dir, old_path ? old_path : "");
    g_setenv("NM_AWG_QUICK_PATH", stubs->quick, TRUE);
    g_setenv("PATH", search_path, TRUE);

    external_stubs_set_installed_module(stubs, NULL);
}

static void
external_stubs_set_tools_version(ExternalStubs *stubs, const char *version)
{
    g_autofree gchar *script = g_strdup_printf("#!/bin/sh\necho 'amneziawg-tools %s - https://amnezia.org'\n", version);

    g_assert_true(g_file_set_contents(stubs->awg, script, -1, NULL));
    g_assert_cmpint(chmod(stubs->awg, 0755), ==, 0);
}

/* NULL: no amneziawg module loaded */
static void
external_stubs_set_module_version(ExternalStubs *stubs, const char *version)
{
    if (version)
        g_assert_true(g_file_set_contents(stubs->module_version, version, -1, NULL));
    else
        unlink(stubs->module_version);
}

static gchar *
external_stubs_take_log(ExternalStubs *stubs)
{
    gchar *contents = NULL;

    if (!g_file_get_contents(stubs->log, &contents, NULL, NULL))
        contents = g_strdup("");
    unlink(stubs->log);
    return contents;
}

static void
external_stubs_teardown(ExternalStubs *stubs)
{
    if (stubs->saved_quick_path)
        g_setenv("NM_AWG_QUICK_PATH", stubs->saved_quick_path, TRUE);
    else
        g_unsetenv("NM_AWG_QUICK_PATH");
    if (stubs->saved_path)
        g_setenv("PATH", stubs->saved_path, TRUE);
    else
        g_unsetenv("PATH");

    unlink(stubs->quick);
    unlink(stubs->awg);
    unlink(stubs->log);
    unlink(stubs->modprobe);
    unlink(stubs->modprobe_log);
    unlink(stubs->module_version);
    rmdir(stubs->dir);
    g_free(stubs->dir);
    g_free(stubs->quick);
    g_free(stubs->awg);
    g_free(stubs->log);
    g_free(stubs->modprobe);
    g_free(stubs->modprobe_log);
    g_free(stubs->module_version);
    g_free(stubs->saved_path);
    g_free(stubs->saved_quick_path);
}

static void
assert_external_connect(ExternalStubs *stubs, AWGDevice *device, gboolean accepted, const char *message_part)
{
    AWGConnectionManager *mgr = external_test_manager(stubs, "awgtest", device, stubs->dir);
    g_autofree gchar *log = NULL;
    GError *error = NULL;

    if (accepted) {
        g_assert_true(awg_connection_manager_connect(mgr, NULL, &error));
        g_assert_no_error(error);
    } else {
        g_assert_false(awg_connection_manager_connect(mgr, NULL, &error));
        g_assert_nonnull(error);
        g_test_message("%s", error->message);
        g_assert_nonnull(strstr(error->message, message_part));
        g_clear_error(&error);
    }
    g_object_unref(mgr);

    /* A refused connection never reaches awg-quick */
    log = external_stubs_take_log(stubs);
    if (accepted)
        g_assert_true(g_str_has_prefix(log, "up "));
    else
        g_assert_cmpstr(log, ==, "");
}

static void
test_external_awg31_requires_new_tools(void)
{
    ExternalStubs stubs = { 0 };
    AWGDevice *awg20 = external_test_device();
    AWGDevice *awg30 = external_test_device();
    AWGDevice *awg31 = external_test_device();

    external_stubs_setup(&stubs);

    /* HeaderProtectionKey .. MaxHandshakeAttempts came with tools 3.0 */
    g_assert_true(awg_device_set_header_protection_key(awg30, "65P9KtswVjU7zVR1f5915+lhDOEi62lMYclBlkP6304="));
    g_assert_true(awg_device_set_content_padding_addition(awg30, "2-10"));
    /* RandomTrailers and DisableCookies only with 3.1 */
    g_assert_true(awg_device_set_content_padding_addition(awg31, "2-10"));
    g_assert_true(awg_device_set_disable_cookies(awg31, TRUE));

    /* Tools without the 3.1 keys must not be asked to parse one ... */
    external_stubs_set_tools_version(&stubs, "v1.0.20260618");
    assert_external_connect(&stubs, awg20, TRUE, NULL);
    assert_external_connect(&stubs, awg30, FALSE, "HeaderProtectionKey, which needs amneziawg-tools 3.0");
    assert_external_connect(&stubs, awg31, FALSE, "DisableCookies, which needs amneziawg-tools 3.1");

    /* ... 3.0 parses all but the two flags ... */
    external_stubs_set_tools_version(&stubs, "v3.0.20260805");
    assert_external_connect(&stubs, awg20, TRUE, NULL);
    assert_external_connect(&stubs, awg30, TRUE, NULL);
    assert_external_connect(&stubs, awg31, FALSE, "DisableCookies, which needs amneziawg-tools 3.1");

    /* ... and 3.1 all of them */
    external_stubs_set_tools_version(&stubs, "v3.1.20260812");
    assert_external_connect(&stubs, awg31, TRUE, NULL);

    g_object_unref(awg20);
    g_object_unref(awg30);
    g_object_unref(awg31);
    external_stubs_teardown(&stubs);
}

/* awg-quick creates the interface through the amneziawg module whenever one
 * can be loaded, so new tools alone are not enough: the module has to know
 * the parameters too, also one that is installed but not loaded yet. No
 * module to load, or one without a readable version, leaves the decision to
 * the tools. */
static void
test_external_awg31_requires_new_module(void)
{
    ExternalStubs stubs = { 0 };
    AWGDevice *awg20 = external_test_device();
    AWGDevice *awg30 = external_test_device();
    AWGDevice *awg31 = external_test_device();
    gchar *modprobe_log = NULL;

    external_stubs_setup(&stubs);
    external_stubs_set_tools_version(&stubs, "v3.1.20260812");
    g_assert_true(awg_device_set_header_protection_key(awg30, "65P9KtswVjU7zVR1f5915+lhDOEi62lMYclBlkP6304="));
    g_assert_true(awg_device_set_random_trailers(awg31, TRUE));

    /* Last 1.x module: none of the 3.1 attributes */
    external_stubs_set_module_version(&stubs, "1.0.20260725\n");
    assert_external_connect(&stubs, awg20, TRUE, NULL);
    assert_external_connect(&stubs, awg30, FALSE,
                            "HeaderProtectionKey, which needs kernel module amneziawg 3.0 or newer, "
                            "but 1.0.20260725 is loaded");
    assert_external_connect(&stubs, awg31, FALSE, "RandomTrailers, which needs kernel module amneziawg 3.1");

    /* 3.0 knows all but the two flags ... */
    external_stubs_set_module_version(&stubs, "3.0.20260730\n");
    assert_external_connect(&stubs, awg30, TRUE, NULL);
    assert_external_connect(&stubs, awg31, FALSE, "RandomTrailers, which needs kernel module amneziawg 3.1");

    /* ... and 3.1 all of them */
    external_stubs_set_module_version(&stubs, "3.1.20260812\n");
    assert_external_connect(&stubs, awg31, TRUE, NULL);

    /* A loaded module is not loaded again */
    modprobe_log = external_stubs_take_modprobe_log(&stubs);
    g_assert_cmpstr(modprobe_log, ==, "");
    g_free(modprobe_log);

    /* Installed but not loaded yet: "ip link add" in awg-quick would load
     * it, so it is loaded first and checked like a loaded one ... */
    external_stubs_set_module_version(&stubs, NULL);
    external_stubs_set_installed_module(&stubs, "1.0.20260725");
    assert_external_connect(&stubs, awg30, FALSE,
                            "HeaderProtectionKey, which needs kernel module amneziawg 3.0 or newer, "
                            "but 1.0.20260725 is loaded");
    modprobe_log = external_stubs_take_modprobe_log(&stubs);
    g_assert_cmpstr(modprobe_log, ==, "amneziawg\n");
    g_free(modprobe_log);

    external_stubs_set_module_version(&stubs, NULL);
    external_stubs_set_installed_module(&stubs, "3.1.20260812");
    assert_external_connect(&stubs, awg31, TRUE, NULL);
    modprobe_log = external_stubs_take_modprobe_log(&stubs);
    g_assert_cmpstr(modprobe_log, ==, "amneziawg\n");
    g_free(modprobe_log);

    /* ... but only for a configuration that needs a version */
    external_stubs_set_module_version(&stubs, NULL);
    assert_external_connect(&stubs, awg20, TRUE, NULL);
    modprobe_log = external_stubs_take_modprobe_log(&stubs);
    g_assert_cmpstr(modprobe_log, ==, "");
    g_free(modprobe_log);

    /* No module to load (awg-quick uses amneziawg-go), or an unknown
     * version: the tools decide */
    external_stubs_set_installed_module(&stubs, NULL);
    assert_external_connect(&stubs, awg31, TRUE, NULL);
    modprobe_log = external_stubs_take_modprobe_log(&stubs);
    g_assert_cmpstr(modprobe_log, ==, "amneziawg\n");
    g_free(modprobe_log);
    external_stubs_set_module_version(&stubs, "\n");
    assert_external_connect(&stubs, awg31, TRUE, NULL);

    /* Both too old: the module is reported first, the tools once it is new */
    external_stubs_set_tools_version(&stubs, "v1.0.20260618");
    external_stubs_set_module_version(&stubs, "1.0.20260725\n");
    assert_external_connect(&stubs, awg30, FALSE, "needs kernel module amneziawg 3.0");
    external_stubs_set_module_version(&stubs, "3.1.20260812\n");
    assert_external_connect(&stubs, awg30, FALSE, "needs amneziawg-tools 3.0");

    g_object_unref(awg20);
    g_object_unref(awg30);
    g_object_unref(awg31);
    external_stubs_teardown(&stubs);
}

/* The refusal for an old module must not send the user to awg-quick, which
 * goes through the same module; it has to name the way out: a newer module,
 * loaded. */
static void
test_netlink_module_check(void)
{
    g_autofree gchar *dir = g_dir_make_tmp("awg-module-XXXXXX", NULL);
    g_autofree gchar *version_file = g_build_filename(dir, "version", NULL);
    g_autofree gchar *missing = g_build_filename(dir, "missing", NULL);
    AWGDevice *awg20 = external_test_device();
    AWGDevice *awg30 = external_test_device();
    GError *error = NULL;

    g_assert_true(awg_device_set_rekey_timeout(awg30, "4-6"));

    g_assert_true(g_file_set_contents(version_file, "1.0.20260725\n", -1, NULL));
    g_assert_true(awg_connection_manager_netlink_check_module(awg20, version_file, &error));
    g_assert_no_error(error);
    g_assert_false(awg_connection_manager_netlink_check_module(awg30, version_file, &error));
    g_assert_nonnull(error);
    g_test_message("%s", error->message);
    g_assert_nonnull(strstr(error->message, "RekeyTimeout, which needs kernel module amneziawg 3.0 or newer, "
                                            "but 1.0.20260725 is loaded"));
    g_assert_nonnull(strstr(error->message, "Upgrade the module and load the new one"));
    g_assert_nonnull(strstr(error->message, "NM_FORCE_AWG_QUICK=1 does not help"));
    g_assert_null(strstr(error->message, "switch to the awg-quick backend"));
    g_clear_error(&error);

    g_assert_true(g_file_set_contents(version_file, "3.0.20260730\n", -1, NULL));
    g_assert_true(awg_connection_manager_netlink_check_module(awg30, version_file, &error));
    g_assert_no_error(error);

    /* Not loaded: the kernel decides */
    g_assert_true(awg_connection_manager_netlink_check_module(awg30, missing, &error));
    g_assert_no_error(error);

    g_object_unref(awg20);
    g_object_unref(awg30);
    unlink(version_file);
    rmdir(dir);
}

/* The configuration lives in the configured directory (/run/nm-amneziawg in
 * the service) only while awg-quick runs, readable by root only, and an
 * interface left over from an earlier activation is taken down first. */
static void
test_external_config_and_leftover_interface(void)
{
    ExternalStubs stubs = { 0 };
    g_autofree gchar *config_dir = NULL;
    g_autofree gchar *conf = NULL;
    g_autofree gchar *lo_conf = NULL;
    g_autofree gchar *expected = NULL;
    g_autofree gchar *log = NULL;
    AWGDevice *device = external_test_device();
    AWGConnectionManager *mgr;
    GError *error = NULL;

    external_stubs_setup(&stubs);
    external_stubs_set_tools_version(&stubs, "v3.1.20260812");
    /* Created on demand */
    config_dir = g_build_filename(stubs.dir, "run", NULL);
    conf = g_build_filename(config_dir, "awgtest-none.conf", NULL);
    lo_conf = g_build_filename(config_dir, "lo.conf", NULL);

    mgr = external_test_manager(&stubs, "awgtest-none", device, config_dir);
    g_assert_true(awg_connection_manager_connect(mgr, NULL, &error));
    g_assert_no_error(error);
    g_assert_false(g_file_test(conf, G_FILE_TEST_EXISTS));
    g_assert_true(awg_connection_manager_disconnect(mgr, &error));
    g_assert_no_error(error);
    g_assert_false(g_file_test(conf, G_FILE_TEST_EXISTS));
    g_object_unref(mgr);

    log = external_stubs_take_log(&stubs);
    expected = g_strdup_printf("up %s 400\ndown %s 400\n", conf, conf);
    g_assert_cmpstr(log, ==, expected);
    g_clear_pointer(&log, g_free);
    g_clear_pointer(&expected, g_free);

    /* "lo" always exists: it stands for a leftover interface */
    mgr = external_test_manager(&stubs, "lo", device, config_dir);
    g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_MESSAGE, "*lo is left over*");
    g_assert_true(awg_connection_manager_connect(mgr, NULL, &error));
    g_test_assert_expected_messages();
    g_assert_no_error(error);
    g_object_unref(mgr);

    log = external_stubs_take_log(&stubs);
    expected = g_strdup_printf("down %s 400\nup %s 400\n", lo_conf, lo_conf);
    g_assert_cmpstr(log, ==, expected);

    g_object_unref(device);
    rmdir(config_dir);
    external_stubs_teardown(&stubs);
}

#define TEST_PRIVATE_KEY "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20="
#define TEST_PSK "2hAJ4eqUN13Ue6DjcLn3MGq6ARllKvI6lzg6Uh62K+w="
#define TEST_PEER_KEY "9rLL/fiLgF39EZnzj1xSwIHrY3G+AIwUtnfDpR2H8uU="

/* @flags < 0 leaves out the "<name>-flags" item. */
static void
vpn_set_secret(NMSettingVpn *s_vpn, const char *name, const char *value, gint flags)
{
    if (value)
        nm_setting_vpn_add_secret(s_vpn, name, value);
    if (flags >= 0)
        nm_setting_set_secret_flags(NM_SETTING(s_vpn), name, flags, NULL);
}

static const AWGSecret *
find_secret(GPtrArray *secrets, const char *name)
{
    for (guint i = 0; i < secrets->len; i++) {
        const AWGSecret *secret = g_ptr_array_index(secrets, i);

        if (g_strcmp0(secret->name, name) == 0)
            return secret;
    }
    return NULL;
}

static void
test_secrets_private_key_flags(void)
{
    static const struct {
        gint flags;
        const char *value;
        gboolean missing;
        gboolean missing_stored;
    } cases[] = {
        /* No flags: stored by NetworkManager, as flags 0 */
        { -1, NULL, TRUE, TRUE },
        { -1, TEST_PRIVATE_KEY, FALSE, FALSE },
        { NM_SETTING_SECRET_FLAG_NONE, NULL, TRUE, TRUE },
        { NM_SETTING_SECRET_FLAG_NONE, "", TRUE, TRUE },
        { NM_SETTING_SECRET_FLAG_NONE, TEST_PRIVATE_KEY, FALSE, FALSE },
        { NM_SETTING_SECRET_FLAG_AGENT_OWNED, NULL, TRUE, FALSE },
        { NM_SETTING_SECRET_FLAG_AGENT_OWNED, TEST_PRIVATE_KEY, FALSE, FALSE },
        { NM_SETTING_SECRET_FLAG_NOT_SAVED, NULL, TRUE, FALSE },
        { NM_SETTING_SECRET_FLAG_NOT_SAVED, TEST_PRIVATE_KEY, FALSE, FALSE },
        { NM_SETTING_SECRET_FLAG_NOT_REQUIRED, NULL, FALSE, FALSE },
        { NM_SETTING_SECRET_FLAG_NOT_REQUIRED, TEST_PRIVATE_KEY, FALSE, FALSE },
    };

    for (guint i = 0; i < G_N_ELEMENTS(cases); i++) {
        NMSettingVpn *s_vpn = NM_SETTING_VPN(nm_setting_vpn_new());
        g_autoptr(GPtrArray) secrets = NULL;
        const AWGSecret *secret;

        nm_setting_vpn_add_data_item(s_vpn, "peer-0-public-key", TEST_PEER_KEY);
        vpn_set_secret(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY, cases[i].value, cases[i].flags);

        secrets = awg_secrets_list(s_vpn);
        g_assert_cmpuint(secrets->len, ==, 1);
        secret = g_ptr_array_index(secrets, 0);
        g_assert_cmpstr(secret->name, ==, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY);
        g_assert_cmpint(secret->peer, ==, -1);
        g_assert_cmpint(secret->flags, ==, cases[i].flags < 0 ? NM_SETTING_SECRET_FLAG_NONE : cases[i].flags);
        g_assert_cmpint(awg_secret_is_missing(secret), ==, cases[i].missing);
        g_assert_cmpint(awg_secret_is_missing_stored(secret), ==, cases[i].missing_stored);
        g_assert_true(awg_secrets_find_missing(secrets) == (cases[i].missing ? secret : NULL));

        g_object_unref(s_vpn);
    }
}

static void
test_secrets_preshared_key_flags(void)
{
    static const struct {
        gint flags;
        const char *value;
        gboolean listed;
        gboolean missing;
        gboolean missing_stored;
    } cases[] = {
        /* No key and no flags: the peer has no preshared key */
        { -1, NULL, FALSE, FALSE, FALSE },
        { -1, TEST_PSK, TRUE, FALSE, FALSE },
        /* An empty value, as nmcli --ask stores for an Enter: still none */
        { -1, "", FALSE, FALSE, FALSE },
        /* Stored by NetworkManager but gone (or written by 0.9.12): asked */
        { NM_SETTING_SECRET_FLAG_NONE, NULL, TRUE, TRUE, TRUE },
        { NM_SETTING_SECRET_FLAG_NONE, "", TRUE, TRUE, TRUE },
        { NM_SETTING_SECRET_FLAG_NONE, TEST_PSK, TRUE, FALSE, FALSE },
        { NM_SETTING_SECRET_FLAG_AGENT_OWNED, NULL, TRUE, TRUE, FALSE },
        { NM_SETTING_SECRET_FLAG_AGENT_OWNED, TEST_PSK, TRUE, FALSE, FALSE },
        { NM_SETTING_SECRET_FLAG_NOT_SAVED, NULL, TRUE, TRUE, FALSE },
        { NM_SETTING_SECRET_FLAG_NOT_SAVED, TEST_PSK, TRUE, FALSE, FALSE },
        { NM_SETTING_SECRET_FLAG_NOT_REQUIRED, NULL, TRUE, FALSE, FALSE },
        { NM_SETTING_SECRET_FLAG_NOT_REQUIRED, TEST_PSK, TRUE, FALSE, FALSE },
    };

    for (guint i = 0; i < G_N_ELEMENTS(cases); i++) {
        NMSettingVpn *s_vpn = NM_SETTING_VPN(nm_setting_vpn_new());
        g_autoptr(GPtrArray) secrets = NULL;
        const AWGSecret *secret;

        nm_setting_vpn_add_secret(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY, TEST_PRIVATE_KEY);
        nm_setting_vpn_add_data_item(s_vpn, "peer-0-public-key", TEST_PEER_KEY);
        vpn_set_secret(s_vpn, "peer-0-preshared-key", cases[i].value, cases[i].flags);

        secrets = awg_secrets_list(s_vpn);
        secret = find_secret(secrets, "peer-0-preshared-key");
        g_test_message("case %u: flags %d, value %s", i, cases[i].flags, cases[i].value ? cases[i].value : "(none)");
        g_assert_cmpint(secret != NULL, ==, cases[i].listed);
        g_assert_cmpint(awg_secrets_has_preshared_key(s_vpn, 0), ==, cases[i].listed);
        if (secret) {
            g_assert_cmpint(secret->peer, ==, 0);
            g_assert_cmpint(awg_secret_is_missing(secret), ==, cases[i].missing);
            g_assert_cmpint(awg_secret_is_missing_stored(secret), ==, cases[i].missing_stored);
        }
        g_assert_true(awg_secrets_find_missing(secrets) == (cases[i].missing ? secret : NULL));

        g_object_unref(s_vpn);
    }
}

static void
test_secrets_several_peers(void)
{
    NMSettingVpn *s_vpn = NM_SETTING_VPN(nm_setting_vpn_new());
    g_autoptr(GPtrArray) secrets = NULL;
    const AWGSecret *missing;

    nm_setting_vpn_add_secret(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY, TEST_PRIVATE_KEY);
    for (guint i = 0; i < 3; i++) {
        g_autofree gchar *key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PUBLIC_KEY, i);
        nm_setting_vpn_add_data_item(s_vpn, key, TEST_PEER_KEY);
    }
    vpn_set_secret(s_vpn, "peer-0-preshared-key", TEST_PSK, NM_SETTING_SECRET_FLAG_NONE);
    /* peer 1 has no preshared key */
    vpn_set_secret(s_vpn, "peer-2-preshared-key", NULL, NM_SETTING_SECRET_FLAG_AGENT_OWNED);
    /* Past the gap at index 3: the service never uses peer 4 */
    nm_setting_vpn_add_data_item(s_vpn, "peer-4-public-key", TEST_PEER_KEY);
    vpn_set_secret(s_vpn, "peer-4-preshared-key", NULL, NM_SETTING_SECRET_FLAG_NONE);
    /* A secret of no peer at all */
    vpn_set_secret(s_vpn, "peer-7-preshared-key", TEST_PSK, NM_SETTING_SECRET_FLAG_NONE);

    secrets = awg_secrets_list(s_vpn);
    g_assert_cmpuint(secrets->len, ==, 3);
    g_assert_cmpstr(((AWGSecret *)g_ptr_array_index(secrets, 0))->name, ==, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY);
    g_assert_cmpstr(((AWGSecret *)g_ptr_array_index(secrets, 1))->name, ==, "peer-0-preshared-key");
    g_assert_cmpstr(((AWGSecret *)g_ptr_array_index(secrets, 2))->name, ==, "peer-2-preshared-key");

    missing = awg_secrets_find_missing(secrets);
    g_assert_nonnull(missing);
    g_assert_cmpstr(missing->name, ==, "peer-2-preshared-key");
    g_assert_cmpint(missing->peer, ==, 2);
    g_clear_pointer(&secrets, g_ptr_array_unref);

    /* The private key is reported first */
    nm_setting_vpn_remove_secret(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY);
    secrets = awg_secrets_list(s_vpn);
    g_assert_cmpstr(awg_secrets_find_missing(secrets)->name, ==, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY);
    g_clear_pointer(&secrets, g_ptr_array_unref);

    vpn_set_secret(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY, TEST_PRIVATE_KEY, -1);
    vpn_set_secret(s_vpn, "peer-2-preshared-key", TEST_PSK, -1);
    secrets = awg_secrets_list(s_vpn);
    g_assert_null(awg_secrets_find_missing(secrets));

    g_object_unref(s_vpn);
}

static void
test_secrets_modify_action(void)
{
    /* NetworkManager keeps system-owned secrets from an agent under
     * modify.own only for a profile restricted to one user. */
    static const struct {
        const char *users[3];
        const char *action;
    } cases[] = {
        { { NULL }, "org.freedesktop.NetworkManager.settings.modify.system" },
        { { "alice", NULL }, "org.freedesktop.NetworkManager.settings.modify.own" },
        { { "alice", "bob", NULL }, "org.freedesktop.NetworkManager.settings.modify.system" },
    };

    NMConnection *connection;

    for (guint i = 0; i < G_N_ELEMENTS(cases); i++) {
        NMSettingConnection *s_con = NM_SETTING_CONNECTION(nm_setting_connection_new());

        connection = nm_simple_connection_new();
        for (guint j = 0; cases[i].users[j]; j++)
            g_assert_true(nm_setting_connection_add_permission(s_con, "user", cases[i].users[j], NULL));
        nm_connection_add_setting(connection, NM_SETTING(s_con));
        g_assert_cmpstr(awg_secrets_modify_action(connection), ==, cases[i].action);
        g_object_unref(connection);
    }

    /* No connection setting at all */
    connection = nm_simple_connection_new();
    g_assert_cmpstr(awg_secrets_modify_action(connection), ==,
                    "org.freedesktop.NetworkManager.settings.modify.system");
    g_object_unref(connection);
}

static void
test_secrets_empty_psk_round_trip(void)
{
    /* A peer whose flags were removed after nmcli --ask stored an empty
     * value: no preshared key, and saving it again keeps it that way. */
    NMConnection *connection = nm_simple_connection_new();
    NMConnection *resaved = nm_simple_connection_new();
    NMSettingVpn *s_vpn = NM_SETTING_VPN(nm_setting_vpn_new());
    NMSettingVpn *s_vpn_resaved;
    g_autoptr(GPtrArray) secrets = NULL;
    AWGDevice *device;
    GError *error = NULL;

    g_object_set(s_vpn, NM_SETTING_VPN_SERVICE_TYPE, NM_AWG_VPN_SERVICE_TYPE, NULL);
    nm_setting_vpn_add_secret(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY, TEST_PRIVATE_KEY);
    nm_setting_vpn_add_data_item(s_vpn, "peer-0-public-key", TEST_PEER_KEY);
    nm_setting_vpn_add_data_item(s_vpn, "peer-0-endpoint", "192.168.1.1:51820");
    nm_setting_vpn_add_data_item(s_vpn, "peer-0-allowed-ips", "0.0.0.0/0");
    vpn_set_secret(s_vpn, "peer-0-preshared-key", "", -1);
    nm_connection_add_setting(connection, NM_SETTING(s_vpn));

    secrets = awg_secrets_list(s_vpn);
    g_assert_null(awg_secrets_find_missing(secrets));
    g_assert_null(find_secret(secrets, "peer-0-preshared-key"));

    device = awg_device_new_from_nm_connection(connection, &error);
    g_assert_no_error(error);
    g_assert_null(awg_device_peer_get_shared_key(awg_device_get_peers_list(device)->data));
    g_assert_cmpint(awg_device_peer_get_shared_key_flags(awg_device_get_peers_list(device)->data), ==,
                    NM_SETTING_SECRET_FLAG_NOT_REQUIRED);

    g_assert_true(awg_device_save_to_nm_connection(device, resaved, &error));
    g_assert_no_error(error);
    s_vpn_resaved = nm_connection_get_setting_vpn(resaved);
    g_assert_null(nm_setting_vpn_get_data_item(s_vpn_resaved, "peer-0-preshared-key-flags"));
    g_assert_null(nm_setting_vpn_get_secret(s_vpn_resaved, "peer-0-preshared-key"));

    g_object_unref(device);
    g_object_unref(resaved);
    g_object_unref(connection);
}

static void
test_apply_edited_psk(void)
{
    static const struct {
        const char *old_key;
        NMSettingSecretFlags old_flags;
        NMSettingSecretFlags chosen;
        const char *psk;
        NMSettingSecretFlags expected;
    } cases[] = {
        /* A peer without a preshared key: an empty field keeps it so */
        { NULL, NM_SETTING_SECRET_FLAG_NOT_REQUIRED, NM_SETTING_SECRET_FLAG_NONE, "", NM_SETTING_SECRET_FLAG_NOT_REQUIRED },
        { NULL, NM_SETTING_SECRET_FLAG_NOT_REQUIRED, NM_SETTING_SECRET_FLAG_AGENT_OWNED, NULL, NM_SETTING_SECRET_FLAG_NOT_REQUIRED },
        { NULL, NM_SETTING_SECRET_FLAG_NOT_REQUIRED, NM_SETTING_SECRET_FLAG_NOT_REQUIRED, "", NM_SETTING_SECRET_FLAG_NOT_REQUIRED },
        /* ... unless it is to be asked for every time */
        { NULL, NM_SETTING_SECRET_FLAG_NOT_REQUIRED, NM_SETTING_SECRET_FLAG_NOT_SAVED, "", NM_SETTING_SECRET_FLAG_NOT_SAVED },
        /* A key typed in is stored as chosen */
        { NULL, NM_SETTING_SECRET_FLAG_NOT_REQUIRED, NM_SETTING_SECRET_FLAG_NONE, TEST_PSK, NM_SETTING_SECRET_FLAG_NONE },
        { NULL, NM_SETTING_SECRET_FLAG_NOT_REQUIRED, NM_SETTING_SECRET_FLAG_AGENT_OWNED, TEST_PSK, NM_SETTING_SECRET_FLAG_AGENT_OWNED },
        /* A key that was shown and cleared is removed, wherever it was kept */
        { TEST_PSK, NM_SETTING_SECRET_FLAG_NONE, NM_SETTING_SECRET_FLAG_NONE, "", NM_SETTING_SECRET_FLAG_NOT_REQUIRED },
        { TEST_PSK, NM_SETTING_SECRET_FLAG_AGENT_OWNED, NM_SETTING_SECRET_FLAG_AGENT_OWNED, "", NM_SETTING_SECRET_FLAG_NOT_REQUIRED },
        { TEST_PSK, NM_SETTING_SECRET_FLAG_NONE, NM_SETTING_SECRET_FLAG_AGENT_OWNED, "", NM_SETTING_SECRET_FLAG_NOT_REQUIRED },
        { TEST_PSK, NM_SETTING_SECRET_FLAG_AGENT_OWNED, NM_SETTING_SECRET_FLAG_NOT_SAVED, "", NM_SETTING_SECRET_FLAG_NOT_SAVED },
        { TEST_PSK, NM_SETTING_SECRET_FLAG_NONE, NM_SETTING_SECRET_FLAG_NONE, TEST_PSK, NM_SETTING_SECRET_FLAG_NONE },
        /* A stored key the editor was not given stays asked for */
        { NULL, NM_SETTING_SECRET_FLAG_NONE, NM_SETTING_SECRET_FLAG_NONE, "", NM_SETTING_SECRET_FLAG_NONE },
        { NULL, NM_SETTING_SECRET_FLAG_AGENT_OWNED, NM_SETTING_SECRET_FLAG_AGENT_OWNED, "", NM_SETTING_SECRET_FLAG_AGENT_OWNED },
        { NULL, NM_SETTING_SECRET_FLAG_NONE, NM_SETTING_SECRET_FLAG_AGENT_OWNED, "", NM_SETTING_SECRET_FLAG_AGENT_OWNED },
        /* ... until "not required" is picked for it */
        { NULL, NM_SETTING_SECRET_FLAG_NONE, NM_SETTING_SECRET_FLAG_NOT_REQUIRED, "", NM_SETTING_SECRET_FLAG_NOT_REQUIRED },
    };
    /* Base64 that is not a 32-byte key: awg_validate_base64() accepts all of
     * them, the setter none. */
    static const char *const rejected[] = {
        "AAAA",
        "2hAJ4eqUN13Ue6DjcLn3MGq6ARllKvI6lzg6Uh62K+",
        "2hAJ4eqUN13Ue6DjcLn3MGq6ARllKvI6lzg6Uh62K+w+AAAA",
    };

    for (guint i = 0; i < G_N_ELEMENTS(cases); i++) {
        AWGDevicePeer *peer = awg_device_peer_new();
        const char *expected_key = cases[i].psk && cases[i].psk[0] ? cases[i].psk : NULL;

        if (cases[i].old_key)
            g_assert_true(awg_device_peer_set_shared_key(peer, cases[i].old_key));
        awg_device_peer_set_shared_key_flags(peer, cases[i].old_flags);

        g_test_message("case %u", i);
        g_assert_true(awg_nm_connection_apply_edited_psk(peer, cases[i].chosen, cases[i].psk));
        g_assert_cmpstr(awg_device_peer_get_shared_key(peer), ==, expected_key);
        g_assert_cmpint(awg_device_peer_get_shared_key_flags(peer), ==, cases[i].expected);

        g_object_unref(peer);
    }

    /* A rejected key changes nothing: a new peer keeps no preshared key
     * (instead of flags 0 without a value, which asks for it forever) and an
     * existing one keeps its key and flags. */
    for (guint i = 0; i < G_N_ELEMENTS(rejected); i++) {
        AWGDevicePeer *peer = awg_device_peer_new();

        g_assert_false(awg_validate_key(rejected[i]));
        g_assert_true(awg_validate_base64(rejected[i]));

        g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*preshared key*");
        g_assert_false(awg_nm_connection_apply_edited_psk(peer, NM_SETTING_SECRET_FLAG_NONE, rejected[i]));
        g_test_assert_expected_messages();
        g_assert_null(awg_device_peer_get_shared_key(peer));
        g_assert_cmpint(awg_device_peer_get_shared_key_flags(peer), ==, NM_SETTING_SECRET_FLAG_NOT_REQUIRED);

        g_assert_true(awg_device_peer_set_shared_key(peer, TEST_PSK));
        awg_device_peer_set_shared_key_flags(peer, NM_SETTING_SECRET_FLAG_AGENT_OWNED);
        g_test_expect_message(G_LOG_DOMAIN, G_LOG_LEVEL_WARNING, "*preshared key*");
        g_assert_false(awg_nm_connection_apply_edited_psk(peer, NM_SETTING_SECRET_FLAG_NONE, rejected[i]));
        g_test_assert_expected_messages();
        g_assert_cmpstr(awg_device_peer_get_shared_key(peer), ==, TEST_PSK);
        g_assert_cmpint(awg_device_peer_get_shared_key_flags(peer), ==, NM_SETTING_SECRET_FLAG_AGENT_OWNED);

        g_object_unref(peer);
    }
}

static void
test_awg_validate_key(void)
{
    g_assert_true(awg_validate_key(TEST_PSK));
    g_assert_true(awg_validate_key("YAnL1JqN5iMHW2kHbNfT9xLqX5vBz1mQWc8p3Kf9R0E="));
    g_assert_false(awg_validate_key(NULL));
    g_assert_false(awg_validate_key(""));
    /* valid base64, but 29, 3 and 21 bytes */
    g_assert_false(awg_validate_key("YWFhYmFhYmFhYmFhYmFhYmFhYmFhYmFhYmFhYmE="));
    g_assert_false(awg_validate_key("AAAA"));
    g_assert_false(awg_validate_key("YAnL1JqN5iMHW2kHbNfT9xLqX5vB"));
    /* not base64 */
    g_assert_false(awg_validate_key("YAnL1JqN5iMHW2kHbNfT9xLqX5vBz1mQWc8p3Kf9R0E=A"));
    g_assert_false(awg_validate_key("YAnL1JqN5iMHW2kHbNfT9xLqX5vBz1mQWc8p3Kf9R0!="));
}

static void
test_import_peer_without_psk(void)
{
    g_autofree gchar *config_path = get_test_config_path("test-config-mixed-psk.conf");
    AWGDevice *device = awg_device_new_from_config(config_path, NULL);
    NMConnection *connection = nm_simple_connection_new();
    NMConnection *resaved = nm_simple_connection_new();
    AWGDevice *restored;
    NMSettingVpn *s_vpn;
    GError *error = NULL;
    g_autoptr(GPtrArray) secrets = NULL;
    const GList *peers;

    g_assert_nonnull(device);
    peers = awg_device_get_peers_list(device);
    g_assert_cmpint(awg_device_peer_get_shared_key_flags(peers->data), ==, NM_SETTING_SECRET_FLAG_NONE);
    g_assert_cmpint(awg_device_peer_get_shared_key_flags(peers->next->data), ==, NM_SETTING_SECRET_FLAG_NOT_REQUIRED);

    g_assert_true(awg_device_save_to_nm_connection(device, connection, &error));
    g_assert_no_error(error);
    s_vpn = nm_connection_get_setting_vpn(connection);

    /* As nmcli imports WireGuard: flags only for the peer that has a key */
    g_assert_cmpstr(nm_setting_vpn_get_data_item(s_vpn, "peer-0-preshared-key-flags"), ==, "0");
    g_assert_cmpstr(nm_setting_vpn_get_secret(s_vpn, "peer-0-preshared-key"), ==, TEST_PSK);
    g_assert_null(nm_setting_vpn_get_data_item(s_vpn, "peer-1-preshared-key-flags"));
    g_assert_null(nm_setting_vpn_get_secret(s_vpn, "peer-1-preshared-key"));

    secrets = awg_secrets_list(s_vpn);
    g_assert_null(awg_secrets_find_missing(secrets));
    g_assert_null(find_secret(secrets, "peer-1-preshared-key"));
    g_clear_pointer(&secrets, g_ptr_array_unref);

    /* Load and save again (what the editor does): still no flags for peer 1 */
    restored = awg_device_new_from_nm_connection(connection, &error);
    g_assert_no_error(error);
    peers = awg_device_get_peers_list(restored);
    g_assert_cmpint(awg_device_peer_get_shared_key_flags(peers->next->data), ==, NM_SETTING_SECRET_FLAG_NOT_REQUIRED);
    g_assert_true(awg_device_save_to_nm_connection(restored, resaved, &error));
    g_assert_no_error(error);
    g_assert_null(nm_setting_vpn_get_data_item(nm_connection_get_setting_vpn(resaved), "peer-1-preshared-key-flags"));
    g_assert_cmpstr(nm_setting_vpn_get_data_item(nm_connection_get_setting_vpn(resaved), "peer-0-preshared-key-flags"), ==, "0");

    /* A stored key that goes missing later is still asked for */
    nm_setting_vpn_remove_secret(s_vpn, "peer-0-preshared-key");
    secrets = awg_secrets_list(s_vpn);
    g_assert_nonnull(awg_secrets_find_missing(secrets));
    g_assert_cmpstr(awg_secrets_find_missing(secrets)->name, ==, "peer-0-preshared-key");

    g_object_unref(restored);
    g_object_unref(resaved);
    g_object_unref(connection);
    g_object_unref(device);
}

static void
test_old_import_psk_flags_stay_required(void)
{
    /* What earlier versions wrote for a peer without PresharedKey. It cannot
     * be told apart from a stored key that got lost, so it keeps asking. */
    NMConnection *connection = nm_simple_connection_new();
    NMConnection *resaved = nm_simple_connection_new();
    NMSettingVpn *s_vpn = NM_SETTING_VPN(nm_setting_vpn_new());
    g_autoptr(GPtrArray) secrets = NULL;
    AWGDevice *device;
    GError *error = NULL;

    g_object_set(s_vpn, NM_SETTING_VPN_SERVICE_TYPE, NM_AWG_VPN_SERVICE_TYPE, NULL);
    nm_setting_vpn_add_secret(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY, TEST_PRIVATE_KEY);
    nm_setting_vpn_add_data_item(s_vpn, "peer-0-public-key", TEST_PEER_KEY);
    nm_setting_vpn_add_data_item(s_vpn, "peer-0-endpoint", "192.168.1.1:51820");
    nm_setting_vpn_add_data_item(s_vpn, "peer-0-allowed-ips", "0.0.0.0/0");
    nm_setting_vpn_add_data_item(s_vpn, "peer-0-preshared-key-flags", "0");
    nm_connection_add_setting(connection, NM_SETTING(s_vpn));

    secrets = awg_secrets_list(s_vpn);
    g_assert_cmpstr(awg_secrets_find_missing(secrets)->name, ==, "peer-0-preshared-key");

    device = awg_device_new_from_nm_connection(connection, &error);
    g_assert_no_error(error);
    g_assert_true(awg_device_save_to_nm_connection(device, resaved, &error));
    g_assert_no_error(error);
    g_assert_cmpstr(nm_setting_vpn_get_data_item(nm_connection_get_setting_vpn(resaved), "peer-0-preshared-key-flags"), ==, "0");

    g_object_unref(device);
    g_object_unref(resaved);
    g_object_unref(connection);
}

static void
test_set_peer_preshared_key(void)
{
    static const struct {
        const char *value;
        NMSettingSecretFlags flags;
        const char *stored_flags;
    } cases[] = {
        { NULL, NM_SETTING_SECRET_FLAG_NOT_REQUIRED, NULL },
        { NULL, NM_SETTING_SECRET_FLAG_NONE, "0" },
        { NULL, NM_SETTING_SECRET_FLAG_AGENT_OWNED, "1" },
        { NULL, NM_SETTING_SECRET_FLAG_NOT_SAVED, "2" },
        { TEST_PSK, NM_SETTING_SECRET_FLAG_NONE, "0" },
        { TEST_PSK, NM_SETTING_SECRET_FLAG_NOT_REQUIRED, "4" },
    };
    AWGDevicePeer *fresh = awg_device_peer_new();

    /* Like nm_wireguard_peer_new(): no preshared key by default */
    g_assert_cmpint(awg_device_peer_get_shared_key_flags(fresh), ==, NM_SETTING_SECRET_FLAG_NOT_REQUIRED);
    g_object_unref(fresh);

    for (guint i = 0; i < G_N_ELEMENTS(cases); i++) {
        NMSettingVpn *s_vpn = NM_SETTING_VPN(nm_setting_vpn_new());
        AWGDevicePeer *peer = awg_device_peer_new();

        if (cases[i].value)
            g_assert_true(awg_device_peer_set_shared_key(peer, cases[i].value));
        awg_device_peer_set_shared_key_flags(peer, cases[i].flags);

        awg_nm_connection_set_peer_preshared_key(s_vpn, 3, peer);
        g_assert_cmpstr(nm_setting_vpn_get_data_item(s_vpn, "peer-3-preshared-key-flags"), ==, cases[i].stored_flags);
        g_assert_cmpstr(nm_setting_vpn_get_secret(s_vpn, "peer-3-preshared-key"), ==, cases[i].value);

        g_object_unref(peer);
        g_object_unref(s_vpn);
    }
}

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/awg/config/ipv4", test_awg_device_new_from_config_ipv4);
    g_test_add_func("/awg/config/ipv6", test_awg_device_new_from_config_ipv6);
    g_test_add_func("/awg/config/dual-stack", test_awg_device_new_from_config_dual_stack);
    g_test_add_func("/awg/config/multi-peer", test_awg_device_new_from_config_multi_peer);
    g_test_add_func("/awg/config/save-load-ipv4", test_awg_device_save_and_load_ipv4);
    g_test_add_func("/awg/config/save-private-permissions", test_awg_device_save_uses_private_permissions);
    g_test_add_func("/awg/config/invalid-value-reported", test_awg_config_invalid_value_is_reported);
    g_test_add_func("/awg/config/incomplete-reported", test_awg_config_incomplete_is_reported);
    g_test_add_func("/awg/validate/i-packet-closing-delimiter",
                    test_awg_i_packet_requires_closing_delimiter);
    g_test_add_func("/awg/config/nm-connection-never-default", test_nm_connection_never_default);
    g_test_add_func("/awg/config/save-load-multi-peer", test_awg_device_save_and_load_multi_peer);
    g_test_add_func("/awg/config/mtu-not-exported-when-zero", test_awg_device_mtu_not_exported_when_zero);
    g_test_add_func("/awg/config/mtu-exported-when-set", test_awg_device_mtu_exported_when_set);
    g_test_add_func("/awg/device/mtu-validation", test_awg_device_mtu_validation);
    g_test_add_func("/awg/device/is-valid", test_awg_device_is_valid);
    g_test_add_func("/awg/subnet/ipv4", test_awg_subnet_ipv4);
    g_test_add_func("/awg/subnet/ipv6", test_awg_subnet_ipv6);
    g_test_add_func("/awg/subnet/ipv6-brackets", test_awg_subnet_ipv6_brackets);
    g_test_add_func("/awg/endpoint/ipv4", test_awg_endpoint_ipv4);
    g_test_add_func("/awg/endpoint/ipv6", test_awg_endpoint_ipv6);
    g_test_add_func("/awg/endpoint/hostname", test_awg_endpoint_hostname);
    g_test_add_func("/awg/endpoint/invalid", test_awg_endpoint_invalid);

    g_test_add_func("/awg/validate/empty", test_awg_validate_empty);
    g_test_add_func("/awg/validate/ip4-valid", test_awg_validate_ip4_valid);
    g_test_add_func("/awg/validate/ip4-invalid", test_awg_validate_ip4_invalid);
    g_test_add_func("/awg/validate/ip6-valid", test_awg_validate_ip6_valid);
    g_test_add_func("/awg/validate/ip6-invalid", test_awg_validate_ip6_invalid);
    g_test_add_func("/awg/validate/port-valid", test_awg_validate_port_valid);
    g_test_add_func("/awg/validate/mtu-valid", test_awg_validate_mtu_valid);
    g_test_add_func("/awg/validate/jc-valid", test_awg_validate_jc_valid);
    g_test_add_func("/awg/validate/jmin-valid", test_awg_validate_jmin_valid);
    g_test_add_func("/awg/validate/jmax-valid", test_awg_validate_jmax_valid);
    g_test_add_func("/awg/validate/base64-valid", test_awg_validate_base64_valid);
    g_test_add_func("/awg/validate/fqdn-valid", test_awg_validate_fqdn_valid);
    g_test_add_func("/awg/validate/subnet-valid", test_awg_validate_subnet_valid);
    g_test_add_func("/awg/validate/allowed-ips-valid", test_awg_validate_allowed_ips_valid);
    g_test_add_func("/awg/validate/endpoint-valid", test_awg_validate_endpoint_valid);
    g_test_add_func("/awg/endpoint/ipv6-empty-brackets", test_awg_endpoint_ipv6_empty_brackets);
    g_test_add_func("/awg/config/no-dns-leak", test_awg_config_string_no_dns_leak);
    g_test_add_func("/awg/config/with-dns", test_awg_config_string_with_dns);
    g_test_add_func("/awg/config/extended", test_awg_device_new_from_config_extended);
    g_test_add_func("/awg/config/save-load-extended", test_awg_device_save_load_extended);
    g_test_add_func("/awg/config/s3-s4", test_awg_config_string_with_s3_s4);
    g_test_add_func("/awg/config/zero-values-omitted", test_awg_config_string_omits_zero_values);
    g_test_add_func("/awg/config/keepalive-off-imports", test_awg_config_keepalive_off_imports);
    g_test_add_func("/awg/config/i1-i5", test_awg_config_string_with_i1_i5);
    g_test_add_func("/awg/config/h1-h4-strings", test_awg_config_string_h1_h4_strings);
    g_test_add_func("/awg/device/s3-s4-setters", test_awg_s3_s4_setters);
    g_test_add_func("/awg/device/i1-i5-setters", test_awg_i1_i5_setters);
    g_test_add_func("/awg/device/h1-h4-strings", test_awg_h1_h4_string_values);
    g_test_add_func("/awg/device/h1-h4-spaces", test_awg_h1_h4_with_spaces);
    g_test_add_func("/awg/device/i1-i5-spaces", test_awg_i1_i5_with_spaces);
    g_test_add_func("/awg/validate/awg-string", test_awg_validate_awg_string);
    g_test_add_func("/awg/validate/magic-header-valid", test_awg_validate_magic_header_valid);
    g_test_add_func("/awg/validate/magic-header-invalid", test_awg_validate_magic_header_invalid);
    g_test_add_func("/awg/validate/i-packet-valid", test_awg_validate_i_packet_valid);
    g_test_add_func("/awg/validate/i-packet-invalid", test_awg_validate_i_packet_invalid);
    g_test_add_func("/awg/validate/s3-valid", test_awg_validate_s3_valid);
    g_test_add_func("/awg/validate/s4-valid", test_awg_validate_s4_valid);
    g_test_add_func("/awg/validate/magic-headers-no-overlap", test_awg_validate_magic_headers_no_overlap);
    g_test_add_func("/awg/validate/jmin-jmax", test_awg_validate_jmin_jmax);
    g_test_add_func("/awg/peer/clone-multi-allowed-ips", test_awg_peer_clone_multi_allowed_ips);
    g_test_add_func("/awg/nm-connection/secrets-roundtrip", test_nm_connection_secrets_roundtrip);
    g_test_add_func("/awg/nm-connection/peer-psk-flags", test_peer_shared_key_flags_roundtrip);
    g_test_add_func("/awg/nm-connection/keyless-config-detected", test_keyless_config_detected);
    g_test_add_func("/awg/device/invalid-reason", test_invalid_reason_messages);
    g_test_add_func("/awg/manager/force-quick-selects-external", test_force_awg_quick_selects_external);
    g_test_add_func("/awg/manager/external-awg31-needs-new-tools", test_external_awg31_requires_new_tools);
    g_test_add_func("/awg/manager/external-awg31-needs-new-module", test_external_awg31_requires_new_module);
    g_test_add_func("/awg/manager/netlink-module-check", test_netlink_module_check);
    g_test_add_func("/awg/manager/external-config-and-leftover", test_external_config_and_leftover_interface);
    g_test_add_func("/awg/validate/range-parse-u32", test_awg_range_parse_u32);
    g_test_add_func("/awg/device/header-protection-key", test_awg_device_header_protection_key);
    g_test_add_func("/awg/device/range-setters-reject-over-u16", test_awg_device_range_setters_reject_over_u16);
    g_test_add_func("/awg/config/awg31-roundtrip", test_awg_config_awg31_roundtrip);
    g_test_add_func("/awg/nm-connection/awg31-roundtrip", test_awg_nm_connection_awg31_roundtrip);
    g_test_add_func("/awg/validate/version-parse", test_awg_version_parse);
    g_test_add_func("/awg/validate/version-at-least", test_awg_version_at_least);
    g_test_add_func("/awg/device/required-awg-version", test_awg_device_required_awg_version);
    g_test_add_func("/awg/secrets/private-key-flags", test_secrets_private_key_flags);
    g_test_add_func("/awg/secrets/preshared-key-flags", test_secrets_preshared_key_flags);
    g_test_add_func("/awg/secrets/several-peers", test_secrets_several_peers);
    g_test_add_func("/awg/secrets/modify-action", test_secrets_modify_action);
    g_test_add_func("/awg/secrets/empty-psk-round-trip", test_secrets_empty_psk_round_trip);
    g_test_add_func("/awg/secrets/apply-edited-psk", test_apply_edited_psk);
    g_test_add_func("/awg/validate/key", test_awg_validate_key);
    g_test_add_func("/awg/secrets/set-peer-preshared-key", test_set_peer_preshared_key);
    g_test_add_func("/awg/secrets/import-peer-without-psk", test_import_peer_without_psk);
    g_test_add_func("/awg/secrets/old-import-psk-flags-stay-required", test_old_import_psk_flags_stay_required);

    return g_test_run();
}
