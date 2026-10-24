/* nm-amneziawg-auth-dialog - secrets helper for NetworkManager agents
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* Secret agents (gnome-shell, nm-applet, nmcli --ask) run the program named
 * by the [GNOME] auth-dialog key of nm-amneziawg-service.name whenever they
 * are asked for the secrets of an AmneziaWG connection. It reads the VPN data
 * and the secrets the agent knows from stdin and never opens a window:
 *
 * - with --external-ui-mode it prints a "VPN Plugin UI" key file describing
 *   the secrets, and the agent draws the prompt itself;
 * - without it, it hands the known secrets back in the old line protocol and
 *   fails when one would have to be typed in.
 *
 * Which secrets exist and which are missing comes from awg_secrets_list(),
 * the function the service answers NeedSecrets with. Modelled on
 * nm-openvpn-auth-dialog. */

#include "config.h"

#include <NetworkManager.h>
#include <glib.h>
#include <glib/gi18n.h>
#include <locale.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "awg/awg-nm-connection.h"
#include "awg/awg-secrets.h"

#define UI_KEYFILE_GROUP "VPN Plugin UI"

/* Seconds to wait for the agent's QUIT, as nm-openvpn-auth-dialog does. */
#define QUIT_TIMEOUT 20

static NMSettingVpn *
setting_from_details(GHashTable *data, GHashTable *secrets)
{
    NMSettingVpn *s_vpn = NM_SETTING_VPN(nm_setting_vpn_new());
    GHashTableIter iter;
    const char *key;
    const char *value;

    g_hash_table_iter_init(&iter, data);
    while (g_hash_table_iter_next(&iter, (gpointer *)&key, (gpointer *)&value)) {
        if (key[0])
            nm_setting_vpn_add_data_item(s_vpn, key, value);
    }

    g_hash_table_iter_init(&iter, secrets);
    while (g_hash_table_iter_next(&iter, (gpointer *)&key, (gpointer *)&value)) {
        if (key[0])
            nm_setting_vpn_add_secret(s_vpn, key, value);
    }

    return s_vpn;
}

/* The value the agent may get back without asking. A secret marked "ask
 * every time" is never reused, even if the agent passed one along. */
static const char *
known_value(NMSettingVpn *s_vpn, const AWGSecret *secret)
{
    if (!secret->has_value || (secret->flags & NM_SETTING_SECRET_FLAG_NOT_SAVED))
        return NULL;

    return nm_setting_vpn_get_secret(s_vpn, secret->name);
}

static gboolean
should_ask(const AWGSecret *secret, gboolean allow_interaction, gboolean reprompt)
{
    if (!allow_interaction || (secret->flags & NM_SETTING_SECRET_FLAG_NOT_REQUIRED))
        return FALSE;

    if (reprompt || (secret->flags & NM_SETTING_SECRET_FLAG_NOT_SAVED))
        return TRUE;

    return awg_secret_is_missing(secret);
}

static gchar *
secret_label(NMSettingVpn *s_vpn, const AWGSecret *secret)
{
    g_autofree gchar *key = NULL;
    const char *peer_name;

    if (secret->peer < 0)
        return g_strdup(_("Private key"));

    key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_ENDPOINT, secret->peer);
    peer_name = nm_setting_vpn_get_data_item(s_vpn, key);
    if (!peer_name || !peer_name[0]) {
        g_free(key);
        key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PUBLIC_KEY, secret->peer);
        peer_name = nm_setting_vpn_get_data_item(s_vpn, key);
    }

    /* TRANSLATORS: %d is the number of the peer as in its peer-N-* keys
     * (from 0), %s its endpoint (host:port) or, without one, its public key */
    return g_strdup_printf(_("Preshared key of peer %d (%s)"), secret->peer, peer_name);
}

/* The prompt text. When a secret NetworkManager should store itself (flags
 * 0) has to be asked for, typing it in may not help: NetworkManager keeps
 * such an answer only from an agent allowed to modify the connection, and
 * which polkit action that takes depends on connection.permissions, which
 * the helper does not get (the service log names it). A
 * preshared key may also not exist at all (version 0.9.12 wrote these flags
 * for every peer), and an empty answer does not say so. Name the ways out;
 * most users see them only here, the service log is off by default. */
static gchar *
describe(GPtrArray *secrets,
         const char *vpn_name,
         const char *vpn_uuid,
         gboolean allow_interaction,
         gboolean reprompt)
{
    GString *description = g_string_new(NULL);
    g_autoptr(GString) no_psk = g_string_new(NULL);
    g_autoptr(GString) restore = g_string_new(NULL);
    guint i;

    g_string_printf(description, _("You need to authenticate to access the Virtual Private Network “%s”."), vpn_name);

    for (i = 0; i < secrets->len; i++) {
        const AWGSecret *secret = g_ptr_array_index(secrets, i);

        if (!awg_secret_is_missing_stored(secret) || !should_ask(secret, allow_interaction, reprompt))
            continue;

        if (secret->peer >= 0)
            g_string_append_printf(no_psk, "\nnmcli connection modify uuid %s -vpn.data %s-flags",
                                   vpn_uuid, secret->name);
        g_string_append_printf(restore, "\nsudo nmcli connection modify uuid %s +vpn.secrets %s=KEY",
                               vpn_uuid, secret->name);
    }

    if (no_psk->len) {
        g_string_append(description, "\n\n");
        g_string_append(description,
                        _("If a peer has no preshared key, choose “The password is not required” for its "
                          "preshared key in the connection editor, or run:"));
        g_string_append(description, no_psk->str);
    }

    if (restore->len) {
        g_string_append(description, "\n\n");
        g_string_append(description,
                        /* TRANSLATORS: keep KEY as it is, the command that follows uses it */
                        _("A key stored for all users that is typed in here is saved only if you may "
                          "modify this connection. Otherwise restore it, with the key in place of KEY:"));
        g_string_append(description, restore->str);
    }

    return g_string_free(description, FALSE);
}

static void
print_external_ui(NMSettingVpn *s_vpn,
                  GPtrArray *secrets,
                  const char *vpn_name,
                  const char *vpn_uuid,
                  gboolean allow_interaction,
                  gboolean reprompt)
{
    g_autoptr(GKeyFile) keyfile = g_key_file_new();
    g_autofree gchar *description = NULL;
    g_autofree gchar *output = NULL;
    guint i;

    /* nm-applet and nmcli expect this group first, nm-applet with Version=2. */
    g_key_file_set_integer(keyfile, UI_KEYFILE_GROUP, "Version", 2);
    g_key_file_set_string(keyfile, UI_KEYFILE_GROUP, "Title", _("Authenticate VPN"));
    description = describe(secrets, vpn_name, vpn_uuid, allow_interaction, reprompt);
    g_key_file_set_string(keyfile, UI_KEYFILE_GROUP, "Description", description);

    for (i = 0; i < secrets->len; i++) {
        const AWGSecret *secret = g_ptr_array_index(secrets, i);
        const char *value = known_value(s_vpn, secret);
        gboolean ask = should_ask(secret, allow_interaction, reprompt);
        g_autofree gchar *label = NULL;

        /* Nothing to hand back and nothing to ask: an empty entry could only
         * end up stored as an empty secret. */
        if (!value && !ask)
            continue;

        label = secret_label(s_vpn, secret);
        g_key_file_set_string(keyfile, secret->name, "Value", value ? value : "");
        g_key_file_set_string(keyfile, secret->name, "Label", label);
        g_key_file_set_boolean(keyfile, secret->name, "IsSecret", TRUE);
        g_key_file_set_boolean(keyfile, secret->name, "ShouldAsk", ask);
        g_key_file_set_boolean(keyfile, secret->name, "ForceEcho", FALSE);
    }

    output = g_key_file_to_data(keyfile, NULL, NULL);
    fputs(output, stdout);
    fflush(stdout);
}

/* The agent answers the end of our reply with QUIT and takes an earlier exit
 * as the end of the request, dropping secrets it has not read yet. */
static void
wait_for_quit(void)
{
    g_autoptr(GString) input = g_string_new(NULL);
    gint64 deadline = g_get_monotonic_time() + QUIT_TIMEOUT * G_USEC_PER_SEC;

    while (!strstr(input->str, "QUIT")) {
        struct pollfd pfd = { .fd = STDIN_FILENO, .events = POLLIN };
        gint64 left_ms = (deadline - g_get_monotonic_time()) / 1000;
        char buf[64];
        ssize_t n;

        if (left_ms <= 0 || poll(&pfd, 1, (int)left_ms) <= 0)
            break;

        n = read(STDIN_FILENO, buf, sizeof(buf));
        if (n <= 0)
            break;

        g_string_append_len(input, buf, n);
        if (input->len > 1024)
            g_string_erase(input, 0, input->len - 8);
    }
}

/* Returns FALSE without printing anything when a secret would have to be
 * typed in: this helper has no window of its own. */
static gboolean
answer_old_protocol(NMSettingVpn *s_vpn,
                    GPtrArray *secrets,
                    gboolean allow_interaction,
                    gboolean reprompt)
{
    guint i;

    for (i = 0; i < secrets->len; i++) {
        if (should_ask(g_ptr_array_index(secrets, i), allow_interaction, reprompt))
            return FALSE;
    }

    for (i = 0; i < secrets->len; i++) {
        const AWGSecret *secret = g_ptr_array_index(secrets, i);
        const char *value = known_value(s_vpn, secret);

        if (value)
            printf("%s\n%s\n", secret->name, value);
    }
    printf("\n\n");
    fflush(stdout);

    wait_for_quit();
    return TRUE;
}

int
main(int argc, char *argv[])
{
    gboolean reprompt = FALSE;
    gboolean allow_interaction = FALSE;
    gboolean external_ui_mode = FALSE;
    g_autofree gchar *vpn_uuid = NULL;
    g_autofree gchar *vpn_name = NULL;
    g_autofree gchar *vpn_service = NULL;
    g_auto(GStrv) hints = NULL;
    GOptionEntry entries[] = {
        { "reprompt", 'r', 0, G_OPTION_ARG_NONE, &reprompt, "Reprompt for secrets", NULL },
        { "uuid", 'u', 0, G_OPTION_ARG_STRING, &vpn_uuid, "UUID of VPN connection", NULL },
        { "name", 'n', 0, G_OPTION_ARG_STRING, &vpn_name, "Name of VPN connection", NULL },
        { "service", 's', 0, G_OPTION_ARG_STRING, &vpn_service, "VPN service type", NULL },
        { "allow-interaction", 'i', 0, G_OPTION_ARG_NONE, &allow_interaction, "Allow user interaction", NULL },
        { "external-ui-mode", 0, 0, G_OPTION_ARG_NONE, &external_ui_mode, "External UI mode", NULL },
        /* The service sends no hints (no SecretsRequired), accepted anyway. */
        { "hint", 't', 0, G_OPTION_ARG_STRING_ARRAY, &hints, "Hints from the VPN plugin", NULL },
        { NULL }
    };
    g_autoptr(GOptionContext) context = NULL;
    g_autoptr(GError) error = NULL;
    g_autoptr(GHashTable) data = NULL;
    g_autoptr(GHashTable) secrets_in = NULL;
    g_autoptr(GPtrArray) secrets = NULL;
    NMSettingVpn *s_vpn;
    int ret = EXIT_SUCCESS;

    /* gnome-shell closes the read end of our stderr: any diagnostic would
     * kill the helper with SIGPIPE before it answers. */
    signal(SIGPIPE, SIG_IGN);

    setlocale(LC_ALL, "");
    bindtextdomain(GETTEXT_PACKAGE, LOCALEDIR);
    bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
    textdomain(GETTEXT_PACKAGE);

    context = g_option_context_new("- AmneziaWG auth dialog");
    g_option_context_add_main_entries(context, entries, NULL);
    if (!g_option_context_parse(context, &argc, &argv, &error)) {
        g_printerr("Error parsing options: %s\n", error->message);
        return EXIT_FAILURE;
    }

    if (!vpn_uuid || !vpn_name || !vpn_service) {
        g_printerr("Have to supply UUID, name, and service\n");
        return EXIT_FAILURE;
    }

    if (strcmp(vpn_service, NM_AWG_VPN_SERVICE_TYPE) != 0) {
        g_printerr("This dialog only works with the '%s' service\n", NM_AWG_VPN_SERVICE_TYPE);
        return EXIT_FAILURE;
    }

    if (!nm_vpn_service_plugin_read_vpn_details(STDIN_FILENO, &data, &secrets_in)) {
        g_printerr("Failed to read '%s' (%s) data and secrets from stdin.\n", vpn_name, vpn_uuid);
        return EXIT_FAILURE;
    }

    s_vpn = setting_from_details(data, secrets_in);
    secrets = awg_secrets_list(s_vpn);

    if (external_ui_mode) {
        print_external_ui(s_vpn, secrets, vpn_name, vpn_uuid, allow_interaction, reprompt);
    } else if (!answer_old_protocol(s_vpn, secrets, allow_interaction, reprompt)) {
        g_printerr("'%s' needs a secret to be entered, which only agents with "
                   "external UI mode support can ask for\n",
                   vpn_name);
        ret = EXIT_FAILURE;
    }

    g_object_unref(s_vpn);
    return ret;
}
