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

/* Runs the built nm-amneziawg-auth-dialog the way secret agents do: options
 * on the command line, the connection on stdin. Usage:
 * nm-amneziawg-auth-dialog-test <path to nm-amneziawg-auth-dialog> */

#include <gio/gio.h>
#include <glib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

#define SERVICE "org.freedesktop.NetworkManager.amneziawg"
#define UUID "0a7e71d3-8d08-4bf7-b233-e8fc3aeaf8e5"
#define KEY "IrQF2MOyaXsmiCEE3FUxejKowR0q65O41dHt3bSTj20="
#define PSK "2hAJ4eqUN13Ue6DjcLn3MGq6ARllKvI6lzg6Uh62K+w="
#define PEER0 "9rLL/fiLgF39EZnzj1xSwIHrY3G+AIwUtnfDpR2H8uU="
#define PEER1 "+0L5dDM5KHhvZmTjWb91oRFhrQ8IrC8aEzFVARXg+Hs="
#define UI "VPN Plugin UI"

static const char *helper_path;

typedef struct {
    gchar *out;
    gchar *err;
    gboolean exited;
    gint status;
} Result;

static void
result_clear(Result *r)
{
    g_free(r->out);
    g_free(r->err);
}

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC(Result, result_clear)

/* @data and @secrets are NULL-terminated key, value lists. */
static gchar *
agent_input(const char *const *data, const char *const *secrets, gboolean quit)
{
    GString *input = g_string_new(NULL);

    for (; data && data[0]; data += 2)
        g_string_append_printf(input, "DATA_KEY=%s\nDATA_VAL=%s\n\n", data[0], data[1]);
    for (; secrets && secrets[0]; secrets += 2)
        g_string_append_printf(input, "SECRET_KEY=%s\nSECRET_VAL=%s\n\n", secrets[0], secrets[1]);
    g_string_append(input, "DONE\n\n");
    if (quit)
        g_string_append(input, "QUIT\n\n");

    return g_string_free(input, FALSE);
}

static GPtrArray *
helper_argv(const char *const *args)
{
    GPtrArray *argv = g_ptr_array_new();

    g_ptr_array_add(argv, (gpointer)helper_path);
    for (; args && *args; args++)
        g_ptr_array_add(argv, (gpointer)*args);
    g_ptr_array_add(argv, NULL);

    return argv;
}

static GSubprocessLauncher *
helper_launcher(GSubprocessFlags flags)
{
    GSubprocessLauncher *launcher = g_subprocess_launcher_new(flags);

    /* Untranslated labels, whatever the build machine has installed */
    g_subprocess_launcher_setenv(launcher, "LC_ALL", "C", TRUE);
    g_subprocess_launcher_unsetenv(launcher, "LANGUAGE");

    return launcher;
}

static void
run(Result *r, const char *const *args, const char *input)
{
    g_autoptr(GPtrArray) argv = helper_argv(args);
    g_autoptr(GSubprocessLauncher) launcher = helper_launcher(G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE);
    g_autoptr(GSubprocess) proc = NULL;
    g_autoptr(GError) error = NULL;

    proc = g_subprocess_launcher_spawnv(launcher, (const char *const *)argv->pdata, &error);
    g_assert_no_error(error);
    g_assert_true(g_subprocess_communicate_utf8(proc, input, NULL, &r->out, &r->err, &error));
    g_assert_no_error(error);
    g_assert_true(g_subprocess_wait(proc, NULL, &error));
    r->exited = g_subprocess_get_if_exited(proc);
    r->status = r->exited ? g_subprocess_get_exit_status(proc) : -1;
}

static GKeyFile *
parse_ui(const Result *r)
{
    g_autoptr(GKeyFile) keyfile = g_key_file_new();
    g_autoptr(GError) error = NULL;
    g_auto(GStrv) groups = NULL;

    g_assert_true(r->exited);
    g_assert_cmpint(r->status, ==, 0);
    g_assert_true(g_key_file_load_from_data(keyfile, r->out, -1, G_KEY_FILE_NONE, &error));
    g_assert_no_error(error);

    /* What nm-applet and nmcli insist on */
    groups = g_key_file_get_groups(keyfile, NULL);
    g_assert_cmpstr(groups[0], ==, UI);
    g_assert_cmpint(g_key_file_get_integer(keyfile, UI, "Version", NULL), ==, 2);
    g_assert_true(g_key_file_has_key(keyfile, UI, "Title", NULL));
    g_assert_true(g_key_file_has_key(keyfile, UI, "Description", NULL));

    return g_steal_pointer(&keyfile);
}

static void
assert_entry(GKeyFile *keyfile, const char *name, const char *value, gboolean ask)
{
    g_autofree gchar *v = g_key_file_get_string(keyfile, name, "Value", NULL);

    g_assert_true(g_key_file_has_group(keyfile, name));
    g_assert_cmpstr(v, ==, value);
    g_assert_cmpint(g_key_file_get_boolean(keyfile, name, "ShouldAsk", NULL), ==, ask);
    g_assert_true(g_key_file_get_boolean(keyfile, name, "IsSecret", NULL));
    g_assert_false(g_key_file_get_boolean(keyfile, name, "ForceEcho", NULL));
}

#define ARGS(...) ((const char *const[]){ "-u", UUID, "-n", "Test VPN", "-s", SERVICE, ##__VA_ARGS__, NULL })
#define LIST(...) ((const char *const[]){ __VA_ARGS__, NULL })

static const char *const two_peers[] = {
    "peer-0-public-key", PEER0,
    "peer-0-endpoint", "vpn.example.com:51820",
    "peer-0-preshared-key-flags", "0",
    "peer-1-public-key", PEER1,
    NULL
};

static void
test_eui_lookup_returns_stored(void)
{
    g_auto(Result) r = { 0 };
    g_autofree gchar *input = agent_input(two_peers, LIST("local-private-key", KEY, "peer-0-preshared-key", PSK), FALSE);
    g_autoptr(GKeyFile) keyfile = NULL;
    g_autofree gchar *description = NULL;

    /* An editor opening the profile: no -i, everything handed back */
    run(&r, ARGS("--external-ui-mode"), input);
    keyfile = parse_ui(&r);
    assert_entry(keyfile, "local-private-key", KEY, FALSE);
    assert_entry(keyfile, "peer-0-preshared-key", PSK, FALSE);
    g_assert_false(g_key_file_has_group(keyfile, "peer-1-preshared-key"));

    description = g_key_file_get_string(keyfile, UI, "Description", NULL);
    g_assert_nonnull(strstr(description, "Test VPN"));
}

static void
test_eui_flags(void)
{
    static const struct {
        const char *flags;
        const char *value;
        gboolean interactive;
        gboolean reprompt;
        gboolean present;
        const char *expect_value;
        gboolean ask;
    } cases[] = {
        /* system-owned: asked only if missing and interaction is allowed */
        { "0", KEY, TRUE, FALSE, TRUE, KEY, FALSE },
        { "0", NULL, TRUE, FALSE, TRUE, "", TRUE },
        { "0", NULL, FALSE, FALSE, FALSE, NULL, FALSE },
        /* agent-owned */
        { "1", KEY, TRUE, FALSE, TRUE, KEY, FALSE },
        { "1", NULL, TRUE, FALSE, TRUE, "", TRUE },
        /* not saved: always asked, a passed value is not reused */
        { "2", KEY, TRUE, FALSE, TRUE, "", TRUE },
        { "2", KEY, FALSE, FALSE, FALSE, NULL, FALSE },
        /* not required: never asked */
        { "4", NULL, TRUE, TRUE, FALSE, NULL, FALSE },
        { "4", KEY, TRUE, TRUE, TRUE, KEY, FALSE },
        /* reprompt asks again, prefilled */
        { "0", KEY, TRUE, TRUE, TRUE, KEY, TRUE },
        { "0", KEY, FALSE, TRUE, TRUE, KEY, FALSE },
    };

    for (guint i = 0; i < G_N_ELEMENTS(cases); i++) {
        g_auto(Result) r = { 0 };
        const char *data[] = { "local-private-key-flags", cases[i].flags, "peer-0-public-key", PEER0, NULL };
        const char *secrets[] = { "local-private-key", cases[i].value, NULL };
        g_autofree gchar *input = agent_input(data, cases[i].value ? secrets : NULL, FALSE);
        g_autoptr(GKeyFile) keyfile = NULL;
        const char *args[10] = { "-u", "uuid", "-n", "Test VPN", "-s", SERVICE, "--external-ui-mode" };
        guint n = 7;

        if (cases[i].interactive)
            args[n++] = "-i";
        if (cases[i].reprompt)
            args[n++] = "-r";

        g_test_message("case %u: flags %s, value %s, -i %d, -r %d", i, cases[i].flags,
                       cases[i].value ? "yes" : "no", cases[i].interactive, cases[i].reprompt);
        run(&r, args, input);
        keyfile = parse_ui(&r);
        if (cases[i].present)
            assert_entry(keyfile, "local-private-key", cases[i].expect_value, cases[i].ask);
        else
            g_assert_false(g_key_file_has_group(keyfile, "local-private-key"));
    }
}

static void
test_eui_preshared_keys(void)
{
    g_auto(Result) r = { 0 };
    const char *const data[] = {
        "peer-0-public-key", PEER0,
        "peer-0-endpoint", "vpn.example.com:51820",
        "peer-0-preshared-key-flags", "1",
        "peer-1-public-key", PEER1,
        "peer-1-preshared-key-flags", "0",
        "peer-2-public-key", PEER0,
        NULL
    };
    g_autofree gchar *input = agent_input(data, LIST("local-private-key", KEY), FALSE);
    g_autoptr(GKeyFile) keyfile = NULL;
    g_autofree gchar *label0 = NULL;
    g_autofree gchar *label1 = NULL;

    run(&r, ARGS("--external-ui-mode", "-i"), input);
    keyfile = parse_ui(&r);
    assert_entry(keyfile, "local-private-key", KEY, FALSE);
    assert_entry(keyfile, "peer-0-preshared-key", "", TRUE);
    /* Flags 0 without a key (an old import or a lost key) is asked too */
    assert_entry(keyfile, "peer-1-preshared-key", "", TRUE);
    g_assert_false(g_key_file_has_group(keyfile, "peer-2-preshared-key"));

    /* Numbered like the peer-N-* keys, so the label matches the key name */
    label0 = g_key_file_get_string(keyfile, "peer-0-preshared-key", "Label", NULL);
    g_assert_cmpstr(label0, ==, "Preshared key of peer 0 (vpn.example.com:51820)");
    label1 = g_key_file_get_string(keyfile, "peer-1-preshared-key", "Label", NULL);
    g_assert_cmpstr(label1, ==, "Preshared key of peer 1 (" PEER1 ")");
}

#define DESCRIPTION "You need to authenticate to access the Virtual Private Network “Test VPN”."
#define REMEDY(name) "\nnmcli connection modify uuid " UUID " -vpn.data " name "-flags"
#define NO_PSK_TEXT "\n\nIf a peer has no preshared key, choose “The password is not required” for its " \
                    "preshared key in the connection editor, or run:"
#define RESTORE_TEXT "\n\nA key stored for all users that is typed in here is saved only if you may " \
                     "modify this connection. Otherwise restore it, with the key in place of KEY:"
#define RESTORE(name) "\nsudo nmcli connection modify uuid " UUID " +vpn.secrets " name "=KEY"

static gchar *
run_description(const char *const *args, const char *const *data, const char *const *secrets)
{
    g_auto(Result) r = { 0 };
    g_autofree gchar *input = agent_input(data, secrets, FALSE);
    g_autoptr(GKeyFile) keyfile = NULL;

    run(&r, args, input);
    keyfile = parse_ui(&r);
    return g_key_file_get_string(keyfile, UI, "Description", NULL);
}

static void
test_eui_missing_stored_psk_hint(void)
{
    /* Flags 0 without a value: what 0.9.12 wrote for peers without a
     * PresharedKey. An empty answer does not mean "none", so the prompt
     * says how to mark such a peer. */
    const char *const data[] = {
        "peer-0-public-key", PEER0,
        "peer-0-preshared-key-flags", "0",
        "peer-1-public-key", PEER1,
        "peer-1-preshared-key-flags", "0",
        "peer-2-public-key", PEER0,
        "peer-2-preshared-key-flags", "1",
        "peer-3-public-key", PEER1,
        NULL
    };
    g_autofree gchar *asked = NULL;
    g_autofree gchar *empty_value = NULL;
    g_autofree gchar *lookup = NULL;
    g_autofree gchar *stored = NULL;
    g_autofree gchar *no_flags = NULL;

    asked = run_description(ARGS("--external-ui-mode", "-i"), data, LIST("local-private-key", KEY));
    g_assert_cmpstr(asked, ==,
                    DESCRIPTION NO_PSK_TEXT REMEDY("peer-0-preshared-key") REMEDY("peer-1-preshared-key")
                        RESTORE_TEXT RESTORE("peer-0-preshared-key") RESTORE("peer-1-preshared-key"));

    /* nmcli --ask sends an empty value when the prompt gets Enter */
    empty_value = run_description(ARGS("--external-ui-mode", "-i"), data,
                                  LIST("local-private-key", KEY, "peer-0-preshared-key", "", "peer-1-preshared-key", PSK));
    g_assert_nonnull(strstr(empty_value, REMEDY("peer-0-preshared-key")));
    g_assert_nonnull(strstr(empty_value, RESTORE("peer-0-preshared-key")));
    g_assert_null(strstr(empty_value, "peer-1-preshared-key"));

    /* Nothing is asked without -i, so there is nothing to explain */
    lookup = run_description(ARGS("--external-ui-mode"), data, LIST("local-private-key", KEY));
    g_assert_cmpstr(lookup, ==, DESCRIPTION);

    /* Stored keys: nothing missing */
    stored = run_description(ARGS("--external-ui-mode", "-i"), data,
                             LIST("local-private-key", KEY, "peer-0-preshared-key", PSK, "peer-1-preshared-key", PSK));
    g_assert_cmpstr(stored, ==, DESCRIPTION);

    /* An empty value without flags is no preshared key at all */
    {
        const char *const data_no_flags[] = { "peer-0-public-key", PEER0, NULL };
        g_auto(Result) r = { 0 };
        g_autofree gchar *input = agent_input(data_no_flags, LIST("local-private-key", KEY, "peer-0-preshared-key", ""), FALSE);
        g_autoptr(GKeyFile) keyfile = NULL;

        run(&r, ARGS("--external-ui-mode", "-i"), input);
        keyfile = parse_ui(&r);
        g_assert_false(g_key_file_has_group(keyfile, "peer-0-preshared-key"));
        no_flags = g_key_file_get_string(keyfile, UI, "Description", NULL);
        g_assert_cmpstr(no_flags, ==, DESCRIPTION);
    }
}

static void
test_eui_missing_stored_private_key_hint(void)
{
    /* A private key stored by NetworkManager (no flags, or 0) that went
     * missing: NetworkManager keeps an answer only from an agent allowed to
     * modify the connection, so the prompt says how to restore it. */
    const char *const no_flags[] = { "peer-0-public-key", PEER0, NULL };
    const char *const flags0[] = { "local-private-key-flags", "0", "peer-0-public-key", PEER0, NULL };
    const char *const agent_owned[] = { "local-private-key-flags", "1", "peer-0-public-key", PEER0, NULL };
    g_autofree gchar *missing = NULL;
    g_autofree gchar *missing0 = NULL;
    g_autofree gchar *user = NULL;
    g_autofree gchar *reprompt = NULL;

    missing = run_description(ARGS("--external-ui-mode", "-i"), no_flags, NULL);
    g_assert_cmpstr(missing, ==, DESCRIPTION RESTORE_TEXT RESTORE("local-private-key"));

    missing0 = run_description(ARGS("--external-ui-mode", "-i"), flags0, NULL);
    g_assert_cmpstr(missing0, ==, DESCRIPTION RESTORE_TEXT RESTORE("local-private-key"));

    /* Kept in the user's keyring: the agent stores what is typed in */
    user = run_description(ARGS("--external-ui-mode", "-i"), agent_owned, NULL);
    g_assert_cmpstr(user, ==, DESCRIPTION);

    /* Reprompting a stored key is no loss */
    reprompt = run_description(ARGS("--external-ui-mode", "-i", "-r"), flags0, LIST("local-private-key", KEY));
    g_assert_cmpstr(reprompt, ==, DESCRIPTION);
}

static void
test_old_protocol_returns_stored(void)
{
    g_auto(Result) r = { 0 };
    g_autofree gchar *input = agent_input(two_peers, LIST("local-private-key", KEY, "peer-0-preshared-key", PSK), TRUE);

    run(&r, ARGS("-i"), input);
    g_assert_true(r.exited);
    g_assert_cmpint(r.status, ==, 0);
    g_assert_cmpstr(r.out, ==, "local-private-key\n" KEY "\npeer-0-preshared-key\n" PSK "\n\n\n");
}

static void
on_wait_done(GObject *source, GAsyncResult *res, gpointer user_data)
{
    gboolean *done = user_data;

    g_subprocess_wait_finish(G_SUBPROCESS(source), res, NULL);
    *done = TRUE;
}

static gboolean
on_timeout(gpointer user_data)
{
    gboolean *timed_out = user_data;

    *timed_out = TRUE;
    return G_SOURCE_REMOVE;
}

/* Runs the main context until @done is set or @ms milliseconds pass. */
static gboolean
wait_for(const gboolean *done, guint ms)
{
    gboolean timed_out = FALSE;
    guint id = g_timeout_add(ms, on_timeout, &timed_out);

    while (!*done && !timed_out)
        g_main_context_iteration(NULL, TRUE);
    if (!timed_out)
        g_source_remove(id);

    return *done;
}

static void
test_old_protocol_waits_for_quit(void)
{
    const char *expected = "local-private-key\n" KEY "\npeer-0-preshared-key\n" PSK "\n\n\n";
    g_autofree gchar *input = agent_input(two_peers, LIST("local-private-key", KEY, "peer-0-preshared-key", PSK), FALSE);
    g_autoptr(GPtrArray) argv = helper_argv(ARGS("-i"));
    g_autoptr(GSubprocessLauncher) launcher = helper_launcher(G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE);
    g_autoptr(GSubprocess) proc = NULL;
    g_autoptr(GError) error = NULL;
    GOutputStream *to_helper;
    GInputStream *from_helper;
    char buf[256] = { 0 };
    gsize n = 0;
    gboolean exited = FALSE;

    proc = g_subprocess_launcher_spawnv(launcher, (const char *const *)argv->pdata, &error);
    g_assert_no_error(error);
    to_helper = g_subprocess_get_stdin_pipe(proc);
    from_helper = g_subprocess_get_stdout_pipe(proc);

    /* stdin stays open: the agent sends QUIT only once it has read the reply */
    g_assert_true(g_output_stream_write_all(to_helper, input, strlen(input), NULL, NULL, &error));
    g_assert_no_error(error);
    g_assert_true(g_input_stream_read_all(from_helper, buf, strlen(expected), &n, NULL, &error));
    g_assert_no_error(error);
    g_assert_cmpstr(buf, ==, expected);

    /* An agent takes the helper's exit as the end of the request */
    g_subprocess_wait_async(proc, NULL, on_wait_done, &exited);
    g_assert_false(wait_for(&exited, 500));

    /* QUIT ends the wait at once, well before its 20 s timeout */
    g_assert_true(g_output_stream_write_all(to_helper, "QUIT\n\n", 6, NULL, NULL, &error));
    g_assert_no_error(error);
    g_assert_true(wait_for(&exited, 10000));
    g_assert_true(g_subprocess_get_if_exited(proc));
    g_assert_cmpint(g_subprocess_get_exit_status(proc), ==, 0);

    g_assert_true(g_input_stream_read_all(from_helper, buf, sizeof(buf), &n, NULL, &error));
    g_assert_no_error(error);
    g_assert_cmpuint(n, ==, 0);
}

static void
test_old_protocol_missing_without_interaction(void)
{
    g_auto(Result) r = { 0 };
    g_autofree gchar *input = agent_input(two_peers, NULL, FALSE);

    /* A plain lookup must not fail: the editor would lose what it has */
    run(&r, ARGS(), input);
    g_assert_true(r.exited);
    g_assert_cmpint(r.status, ==, 0);
    g_assert_cmpstr(r.out, ==, "\n\n");
}

static void
test_old_protocol_cannot_ask(void)
{
    g_auto(Result) r = { 0 };
    const char *const data[] = { "local-private-key-flags", "2", "peer-0-public-key", PEER0, NULL };
    g_autofree gchar *input = agent_input(data, NULL, FALSE);

    run(&r, ARGS("-i"), input);
    g_assert_true(r.exited);
    g_assert_cmpint(r.status, !=, 0);
    g_assert_cmpstr(r.out, ==, "");
}

static void
test_rejects_bad_invocation(void)
{
    /* Both are refused before stdin is read, so none is sent */
    {
        g_auto(Result) r = { 0 };
        const char *args[] = { "-u", "uuid", "-n", "Test VPN", "-s", "org.freedesktop.NetworkManager.openvpn", "--external-ui-mode", NULL };

        run(&r, args, NULL);
        g_assert_true(r.exited);
        g_assert_cmpint(r.status, ==, 1);
        g_assert_cmpstr(r.out, ==, "");
    }
    {
        g_auto(Result) r = { 0 };
        const char *args[] = { "-n", "Test VPN", "-s", SERVICE, "--external-ui-mode", NULL };

        run(&r, args, NULL);
        g_assert_true(r.exited);
        g_assert_cmpint(r.status, ==, 1);
    }
    {
        g_auto(Result) r = { 0 };

        run(&r, ARGS("--external-ui-mode"), "");
        g_assert_true(r.exited);
        g_assert_cmpint(r.status, ==, 1);
        g_assert_cmpstr(r.out, ==, "");
    }
}

static void
test_survives_closed_stderr(void)
{
    /* gnome-shell closes the read end of the helper's stderr */
    const char *const data[] = { "local-private-key-flags", "2", "peer-0-public-key", PEER0, NULL };
    g_autofree gchar *input = agent_input(data, NULL, FALSE);
    g_autoptr(GPtrArray) argv = helper_argv(ARGS("-i"));
    g_autoptr(GSubprocessLauncher) launcher = helper_launcher(G_SUBPROCESS_FLAGS_STDIN_PIPE | G_SUBPROCESS_FLAGS_STDOUT_PIPE);
    g_autoptr(GSubprocess) proc = NULL;
    g_autoptr(GError) error = NULL;
    g_autofree gchar *out = NULL;
    int fds[2];

    /* GSubprocess closes the read end in the child, only ours is left */
    g_assert_cmpint(pipe(fds), ==, 0);
    g_subprocess_launcher_take_stderr_fd(launcher, fds[1]);
    proc = g_subprocess_launcher_spawnv(launcher, (const char *const *)argv->pdata, &error);
    g_assert_no_error(error);
    close(fds[0]);

    /* The failure message is written only after stdin was read */
    g_assert_true(g_subprocess_communicate_utf8(proc, input, NULL, &out, NULL, &error));
    g_assert_no_error(error);
    g_assert_true(g_subprocess_wait(proc, NULL, &error));
    g_assert_false(g_subprocess_get_if_signaled(proc));
    g_assert_true(g_subprocess_get_if_exited(proc));
    g_assert_cmpint(g_subprocess_get_exit_status(proc), ==, 1);
}

int
main(int argc, char *argv[])
{
    /* A helper that exits early must not take the test down with it */
    signal(SIGPIPE, SIG_IGN);
    g_test_init(&argc, &argv, NULL);

    if (argc < 2) {
        g_printerr("usage: %s <nm-amneziawg-auth-dialog>\n", argv[0]);
        return 1;
    }
    helper_path = argv[1];

    g_test_add_func("/auth-dialog/external-ui/lookup-returns-stored", test_eui_lookup_returns_stored);
    g_test_add_func("/auth-dialog/external-ui/flags", test_eui_flags);
    g_test_add_func("/auth-dialog/external-ui/preshared-keys", test_eui_preshared_keys);
    g_test_add_func("/auth-dialog/external-ui/missing-stored-psk-hint", test_eui_missing_stored_psk_hint);
    g_test_add_func("/auth-dialog/external-ui/missing-stored-private-key-hint", test_eui_missing_stored_private_key_hint);
    g_test_add_func("/auth-dialog/old-protocol/returns-stored", test_old_protocol_returns_stored);
    g_test_add_func("/auth-dialog/old-protocol/waits-for-quit", test_old_protocol_waits_for_quit);
    g_test_add_func("/auth-dialog/old-protocol/missing-without-interaction", test_old_protocol_missing_without_interaction);
    g_test_add_func("/auth-dialog/old-protocol/cannot-ask", test_old_protocol_cannot_ask);
    g_test_add_func("/auth-dialog/rejects-bad-invocation", test_rejects_bad_invocation);
    g_test_add_func("/auth-dialog/survives-closed-stderr", test_survives_closed_stderr);

    return g_test_run();
}
