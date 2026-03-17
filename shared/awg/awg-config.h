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

#ifndef AWG_CONFIG_H
#define AWG_CONFIG_H

#include "awg-device.h"

#define AWG_CONFIG_INTERFACE_SECTION "[Interface]"
#define AWG_CONFIG_PEER_SECTION "[Peer]"

#define AWG_CONFIG_DEVICE_PRIVATE_KEY "PrivateKey"
#define AWG_CONFIG_DEVICE_PUBLIC_KEY "PublicKey"
#define AWG_CONFIG_DEVICE_PUBLIC_KEY_COMMENTED "#_PublicKey"
#define AWG_CONFIG_DEVICE_ADDRESS "Address"
#define AWG_CONFIG_DEVICE_DNS "DNS"
#define AWG_CONFIG_DEVICE_FW_MARK "FwMark"
#define AWG_CONFIG_DEVICE_LISTEN_PORT "ListenPort"
#define AWG_CONFIG_DEVICE_JC "Jc"
#define AWG_CONFIG_DEVICE_JMIN "Jmin"
#define AWG_CONFIG_DEVICE_JMAX "Jmax"
#define AWG_CONFIG_DEVICE_S1 "S1"
#define AWG_CONFIG_DEVICE_S2 "S2"
#define AWG_CONFIG_DEVICE_H1 "H1"
#define AWG_CONFIG_DEVICE_H2 "H2"
#define AWG_CONFIG_DEVICE_H3 "H3"
#define AWG_CONFIG_DEVICE_H4 "H4"
#define AWG_CONFIG_DEVICE_MTU "MTU"
#define AWG_CONFIG_DEVICE_PRE_UP "PreUp"
#define AWG_CONFIG_DEVICE_POST_UP "PostUp"
#define AWG_CONFIG_DEVICE_PRE_DOWN "PreDown"
#define AWG_CONFIG_DEVICE_POST_DOWN "PostDown"

#define AWG_CONFIG_PEER_PUBLIC_KEY "PublicKey"
#define AWG_CONFIG_PEER_PRESHARED_KEY "PresharedKey"
#define AWG_CONFIG_PEER_ALLOWED_IPS "AllowedIPs"
#define AWG_CONFIG_PEER_ENDPOINT "Endpoint"
#define AWG_CONFIG_PEER_KEEP_ALIVE "PersistentKeepalive"
#define AWG_CONFIG_PEER_ADVANCED_SECURITY "AdvancedSecurity"

AWGDevice *awg_device_new_from_config(const char *config_path);
gchar *awg_device_create_config_string(AWGDevice *device);
gboolean awg_device_save_to_file(AWGDevice *device, const char *config_path);

#endif // AWG_CONFIG_H
