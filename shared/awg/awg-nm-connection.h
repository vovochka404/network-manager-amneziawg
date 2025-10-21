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

#ifndef AWG_NM_CONNECTION_H
#define AWG_NM_CONNECTION_H

#include "awg-device.h"
#include <NetworkManager.h>
#include <gio/gio.h>
#include <glib.h>

#define NM_AWG_VPN_SERVICE_TYPE "org.freedesktop.NetworkManager.amneziawg"

#define NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY "local-private-key"
#define NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY_FLAGS "local-private-key-flags"
#define NM_AWG_VPN_CONFIG_DEVICE_PUBLIC_KEY "peer-public-key"
#define NM_AWG_VPN_CONFIG_DEVICE_LISTEN_PORT "local-listen-port"
#define NM_AWG_VPN_CONFIG_DEVICE_FW_MARK "interface-fw-mark"
#define NM_AWG_VPN_CONFIG_DEVICE_JC "connection-jc"
#define NM_AWG_VPN_CONFIG_DEVICE_JMIN "connection-jmin"
#define NM_AWG_VPN_CONFIG_DEVICE_JMAX "connection-jmax"
#define NM_AWG_VPN_CONFIG_DEVICE_S1 "connection-s1"
#define NM_AWG_VPN_CONFIG_DEVICE_S2 "connection-s2"
#define NM_AWG_VPN_CONFIG_DEVICE_H1 "connection-h1"
#define NM_AWG_VPN_CONFIG_DEVICE_H2 "connection-h2"
#define NM_AWG_VPN_CONFIG_DEVICE_H3 "connection-h3"
#define NM_AWG_VPN_CONFIG_DEVICE_H4 "connection-h4"
#define NM_AWG_VPN_CONFIG_DEVICE_MTU "interface-mtu"
#define NM_AWG_VPN_CONFIG_DEVICE_PRE_UP "script-pre-up"
#define NM_AWG_VPN_CONFIG_DEVICE_POST_UP "script-post-up"
#define NM_AWG_VPN_CONFIG_DEVICE_PRE_DOWN "script-pre-down"
#define NM_AWG_VPN_CONFIG_DEVICE_POST_DOWN "script-post-down"

#define NM_AWG_VPN_CONFIG_PEER_PUBLIC_KEY "peer-%d-public-key"
#define NM_AWG_VPN_CONFIG_PEER_PRESHARED_KEY "peer-%d-preshared-key"
#define NM_AWG_VPN_CONFIG_PEER_PRESHARED_KEY_FLAGS "peer-%d-preshared-key-flags"
#define NM_AWG_VPN_CONFIG_PEER_ALLOWED_IPS "peer-%d-allowed-ips"
#define NM_AWG_VPN_CONFIG_PEER_ENDPOINT "peer-%d-endpoint"
#define NM_AWG_VPN_CONFIG_PEER_KEEP_ALIVE "peer-%d-keep-alive"
#define NM_AWG_VPN_CONFIG_PEER_ADVANCED_SECURITY "peer-%d-advanced-security"

AWGDevice *awg_device_new_from_nm_connection(NMConnection *connection, GError **error);
gboolean awg_device_save_to_nm_connection(AWGDevice *device, NMConnection *connection, GError **error);

#endif // AWG_NM_CONNECTION_H
