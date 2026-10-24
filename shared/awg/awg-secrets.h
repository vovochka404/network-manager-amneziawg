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

#ifndef AWG_SECRETS_H
#define AWG_SECRETS_H

#include <NetworkManager.h>
#include <glib.h>

/* One secret an AmneziaWG connection uses: the interface private key or the
 * preshared key of a peer. */
typedef struct {
    gchar *name; /* key in vpn.secrets */
    gint peer;   /* peer index, -1 for the interface private key */
    NMSettingSecretFlags flags;
    gboolean has_value; /* vpn.secrets holds a non-empty value */
} AWGSecret;

GPtrArray *awg_secrets_list(NMSettingVpn *s_vpn);
gboolean awg_secrets_has_preshared_key(NMSettingVpn *s_vpn, guint peer);
gboolean awg_secret_is_missing(const AWGSecret *secret);
gboolean awg_secret_is_missing_stored(const AWGSecret *secret);
const AWGSecret *awg_secrets_find_missing(GPtrArray *secrets);
const char *awg_secrets_modify_action(NMConnection *connection);

#endif // AWG_SECRETS_H
