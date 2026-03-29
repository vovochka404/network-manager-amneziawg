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
#include "awg/awg-device.h"
#include "awg/awg-validate.h"
#include <glib.h>
#include <stdio.h>
#include <string.h>
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
    AWGDevice *device = awg_device_new_from_config(config_path);

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

    g_object_unref(device);
    g_free(config_path);
}

static void
test_awg_device_new_from_config_ipv6(void)
{
    gchar *config_path = get_test_config_path("test-config-ipv6.conf");
    AWGDevice *device = awg_device_new_from_config(config_path);

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
    AWGDevice *device = awg_device_new_from_config(config_path);

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
    AWGDevice *device = awg_device_new_from_config(config_path);

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
    AWGDevice *device = awg_device_new_from_config(config_path);
    g_assert_nonnull(device);

    gchar *output_path = g_build_filename(g_get_tmp_dir(), "output-ipv4.conf", NULL);
    gboolean saved = awg_device_save_to_file(device, output_path);
    g_assert_true(saved);

    g_object_unref(device);

    AWGDevice *device2 = awg_device_new_from_config(output_path);
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
test_awg_device_save_and_load_multi_peer(void)
{
    gchar *config_path = get_test_config_path("test-config-multi-peer.conf");
    AWGDevice *device = awg_device_new_from_config(config_path);
    g_assert_nonnull(device);

    gchar *output_path = g_build_filename(g_get_tmp_dir(), "output-multi-peer.conf", NULL);
    gboolean saved = awg_device_save_to_file(device, output_path);
    g_assert_true(saved);

    g_object_unref(device);

    AWGDevice *device2 = awg_device_new_from_config(output_path);
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
    AWGDevice *device = awg_device_new_from_config(config_path);
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
    g_assert_true(awg_validate_endpoint("vpn.example.com"));
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
    AWGDevice *device = awg_device_new_from_config(config_path);

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
    AWGDevice *device = awg_device_new_from_config(config_path);
    g_assert_nonnull(device);

    gchar *output_path = g_build_filename(g_get_tmp_dir(), "output-extended.conf", NULL);
    gboolean saved = awg_device_save_to_file(device, output_path);
    g_assert_true(saved);

    g_object_unref(device);

    AWGDevice *device2 = awg_device_new_from_config(output_path);
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

int
main(int argc, char *argv[])
{
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/awg/config/ipv4", test_awg_device_new_from_config_ipv4);
    g_test_add_func("/awg/config/ipv6", test_awg_device_new_from_config_ipv6);
    g_test_add_func("/awg/config/dual-stack", test_awg_device_new_from_config_dual_stack);
    g_test_add_func("/awg/config/multi-peer", test_awg_device_new_from_config_multi_peer);
    g_test_add_func("/awg/config/save-load-ipv4", test_awg_device_save_and_load_ipv4);
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

    return g_test_run();
}
