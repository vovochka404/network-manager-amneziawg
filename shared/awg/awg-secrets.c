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

#include "awg-secrets.h"
#include "awg-nm-connection.h"

static AWGSecret *
secret_new(NMSettingVpn *s_vpn, gchar *name, gint peer)
{
    AWGSecret *secret = g_new0(AWGSecret, 1);
    const char *value = nm_setting_vpn_get_secret(s_vpn, name);

    secret->name = name;
    secret->peer = peer;
    secret->has_value = value && *value;
    nm_setting_get_secret_flags(NM_SETTING(s_vpn), name, &secret->flags, NULL);

    return secret;
}

static void
secret_free(gpointer data)
{
    AWGSecret *secret = data;

    g_free(secret->name);
    g_free(secret);
}

/* Whether peer @peer has a preshared key at all: its flags are set or a
 * non-empty key is stored. Neither means no preshared key, the way
 * awg_nm_connection_set_peer_preshared_key() writes such a peer. An empty
 * stored value does not count: nmcli --ask sends one when its prompt is
 * answered empty, and NetworkManager stores it. */
gboolean
awg_secrets_has_preshared_key(NMSettingVpn *s_vpn, guint peer)
{
    g_autofree gchar *name = NULL;
    g_autofree gchar *flags_key = NULL;
    const char *value;

    g_return_val_if_fail(NM_IS_SETTING_VPN(s_vpn), FALSE);

    name = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PRESHARED_KEY, peer);
    value = nm_setting_vpn_get_secret(s_vpn, name);
    if (value && value[0])
        return TRUE;

    /* Not nm_setting_get_secret_flags(): it also succeeds without flags
     * whenever a value, even an empty one, is stored. */
    flags_key = g_strdup_printf("%s-flags", name);
    return nm_setting_vpn_get_data_item(s_vpn, flags_key) != NULL;
}

/* Lists the secrets of @s_vpn: always the private key, and the preshared key
 * of every peer that has one. Peer indexes are walked like
 * awg_device_new_from_nm_connection() walks them, consecutively up to the
 * first one without a public key. The checks that make the device leave a
 * peer out (an invalid public key, no endpoint or AllowedIPs) are not
 * repeated here: the editor and the import never write such a peer, so only
 * hand-edited vpn.data has one, and its key is asked for.
 *
 * This is the single source of truth for NeedSecrets in the service and for
 * the prompt of the auth dialog. */
GPtrArray *
awg_secrets_list(NMSettingVpn *s_vpn)
{
    GPtrArray *secrets;
    guint i;

    g_return_val_if_fail(NM_IS_SETTING_VPN(s_vpn), NULL);

    secrets = g_ptr_array_new_with_free_func(secret_free);
    g_ptr_array_add(secrets, secret_new(s_vpn, g_strdup(NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY), -1));

    for (i = 0; i < NM_AWG_MAX_PEERS; i++) {
        g_autofree gchar *public_key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PUBLIC_KEY, i);

        if (!nm_setting_vpn_get_data_item(s_vpn, public_key))
            break;

        if (!awg_secrets_has_preshared_key(s_vpn, i))
            continue;

        g_ptr_array_add(secrets, secret_new(s_vpn, g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PRESHARED_KEY, i), (gint)i));
    }

    return secrets;
}

/* A required secret without a value. Flags 0 (stored by NetworkManager)
 * count as required too: a stored key that went missing must be asked for,
 * not silently dropped. */
gboolean
awg_secret_is_missing(const AWGSecret *secret)
{
    g_return_val_if_fail(secret, FALSE);

    return !secret->has_value && !(secret->flags & NM_SETTING_SECRET_FLAG_NOT_REQUIRED);
}

/* A secret NetworkManager should store itself (flags 0, "for all users")
 * that has no value. Either the stored key got lost, or, for a preshared
 * key, the profile was written by version 0.9.12, which set these flags for
 * peers without a PresharedKey too. It is asked for like any missing secret,
 * but NetworkManager keeps an answer only from an agent allowed to modify
 * the connection (see awg_secrets_modify_action()), so callers name the way
 * out as well. */
gboolean
awg_secret_is_missing_stored(const AWGSecret *secret)
{
    g_return_val_if_fail(secret, FALSE);

    return secret->flags == NM_SETTING_SECRET_FLAG_NONE && !secret->has_value;
}

const AWGSecret *
awg_secrets_find_missing(GPtrArray *secrets)
{
    guint i;

    g_return_val_if_fail(secrets, NULL);

    for (i = 0; i < secrets->len; i++) {
        const AWGSecret *secret = g_ptr_array_index(secrets, i);

        if (awg_secret_is_missing(secret))
            return secret;
    }

    return NULL;
}

/* The polkit action NetworkManager checks before it keeps a secret stored
 * for all users (flags 0) that a secret agent returns, or hands an existing
 * one to it: modify.own for a profile restricted to one user, modify.system
 * for any other (nm-agent-manager.c, nm-settings-connection.c). */
const char *
awg_secrets_modify_action(NMConnection *connection)
{
    NMSettingConnection *s_con;

    g_return_val_if_fail(NM_IS_CONNECTION(connection), NULL);

    s_con = nm_connection_get_setting_connection(connection);
    if (s_con && nm_setting_connection_get_num_permissions(s_con) == 1)
        return "org.freedesktop.NetworkManager.settings.modify.own";

    return "org.freedesktop.NetworkManager.settings.modify.system";
}
