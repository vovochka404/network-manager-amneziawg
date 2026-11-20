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

#include "awg-connection-manager-external.h"
#include "amneziawg.h"
#include "awg-config.h"
#include "awg-connection-manager-netlink.h"
#include "awg-validate.h"
#include "config.h"
#include <errno.h>
#include <fcntl.h>
#include <glib.h>
#include <net/if.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

/* awg-quick configs go to AWG_EXTERNAL_CONFIG_DIR (config.h), not /tmp: NetworkManager
 * 1.58+ has PrivateTmp=true, and with KillMode=process this process outlives an NM
 * restart and tears the tunnel down after that private /tmp is gone. */

typedef struct _AWGConnectionManagerExternalPrivate AWGConnectionManagerExternalPrivate;

struct _AWGConnectionManagerExternal {
    GObject parent_instance;
};

struct _AWGConnectionManagerExternalClass {
    GObjectClass parent_class;
};

struct _AWGConnectionManagerExternalPrivate {
    gchar *interface_name;
    AWGDevice *device;
    gchar *config_dir;
    gchar *conf_path;
    gchar *module_version_file;
    /* Of the link "awg-quick up" created: the name may pass to a newer activation */
    unsigned int ifindex;
};

enum {
    PROP_0,
    PROP_INTERFACE_NAME,
    PROP_DEVICE,
    PROP_CONFIG_DIR,
    PROP_MODULE_VERSION_FILE,
    N_PROPS
};

static void awg_connection_manager_external_interface_init(AWGConnectionManagerInterface *iface);

G_DEFINE_TYPE_WITH_CODE(AWGConnectionManagerExternal, awg_connection_manager_external, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(AWG_TYPE_CONNECTION_MANAGER, awg_connection_manager_external_interface_init) G_ADD_PRIVATE(AWGConnectionManagerExternal))

#define AWG_CONNECTION_MANAGER_EXTERNAL_GET_PRIVATE(self) \
    ((AWGConnectionManagerExternalPrivate *)awg_connection_manager_external_get_instance_private((AWGConnectionManagerExternal *)(self)))

#define AWG_CONNECTION_MANAGER_ERROR g_quark_from_static_string("awg-connection-manager-error-quark")

static const gchar *
awg_quick_find_exe(void)
{
    const gchar *env_path = g_getenv("NM_AWG_QUICK_PATH");
    if (env_path && g_file_test(env_path, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_EXECUTABLE))
        return env_path;

    static const gchar *paths[] = {
        "/usr/local/sbin/awg-quick",
        "/usr/local/bin/awg-quick",
        "/usr/sbin/awg-quick",
        "/usr/bin/awg-quick",
        "/sbin/awg-quick",
        "/bin/awg-quick",
        NULL
    };
    for (int i = 0; paths[i]; ++i) {
        if (g_file_test(paths[i], G_FILE_TEST_EXISTS | G_FILE_TEST_IS_EXECUTABLE))
            return paths[i];
    }
    return NULL;
}

gboolean
awg_connection_manager_external_is_available(void)
{
    return awg_quick_find_exe() != NULL;
}

/* AmneziaWG 3.1 parameters and PersistentKeepalive ranges do not exist in the
 * parsers shipped before amneziawg-tools 3.0, RandomTrailers and
 * DisableCookies not before 3.1: awg-quick feeds the generated file to "awg
 * setconf", which rejects an unknown key or value with "Line unrecognized" and
 * aborts the activation without saying which tool is too old. Check the
 * version of the binary awg-quick will actually call, the one found in PATH,
 * like the script itself resolves it. An unreadable or unparseable version is
 * not blocked; the command then reports its own failure, as before. */
static gboolean
external_check_awg31_support(AWGDevice *device, GError **error)
{
    g_autofree gchar *awg = NULL;
    g_autofree gchar *output = NULL;
    g_autofree gchar *command = NULL;
    GError *spawn_error = NULL;
    gint exit_status = 0;
    const gchar *param = NULL;
    guint major = 0;
    guint minor = 0;

    if (!awg_device_get_required_awg_tools_version(device, &major, &minor, &param))
        return TRUE;

    awg = g_find_program_in_path("awg");
    if (!awg)
        return TRUE;

    command = g_strdup_printf("%s --version", awg);
    if (!g_spawn_command_line_sync(command, &output, NULL, &exit_status, &spawn_error) ||
        !g_spawn_check_exit_status(exit_status, NULL)) {
        g_clear_error(&spawn_error);
        return TRUE;
    }
    g_clear_error(&spawn_error);

    if (!output)
        return TRUE;

    g_strstrip(output);
    if (awg_version_at_least(output, major, minor))
        return TRUE;

    /* Only the tools need 3.0 for a keepalive range: any module takes it */
    if (!awg_device_get_required_awg_version(device, NULL, NULL, NULL)) {
        g_set_error(error, AWG_CONNECTION_MANAGER_ERROR, 0,
                    "This connection uses a %s, which needs amneziawg-tools %u.%u or newer, but "
                    "%s reports \"%s\". Upgrade amneziawg-tools, or, with the amneziawg kernel "
                    "module installed, use the kernel backend (unset NM_FORCE_AWG_QUICK)",
                    param, major, minor, awg, output);
        return FALSE;
    }

    g_set_error(error, AWG_CONNECTION_MANAGER_ERROR, 0,
                "This connection uses the AmneziaWG 3.1 parameter %s, which needs amneziawg-tools "
                "%u.%u or newer, but %s reports \"%s\". Upgrade amneziawg-tools, or use the kernel "
                "backend with amneziawg module %u.%u or newer (unset NM_FORCE_AWG_QUICK)",
                param, major, minor, awg, output, major, minor);
    return FALSE;
}

static gchar *
find_system_program(const gchar *name)
{
    static const gchar *sbin_dirs[] = { "/usr/sbin", "/sbin" };
    gchar *path = g_find_program_in_path(name);

    /* PATH may lack sbin when NetworkManager runs with a reduced environment */
    for (guint i = 0; !path && i < G_N_ELEMENTS(sbin_dirs); i++) {
        g_autofree gchar *candidate = g_build_filename(sbin_dirs[i], name, NULL);

        if (g_file_test(candidate, G_FILE_TEST_IS_EXECUTABLE))
            path = g_steal_pointer(&candidate);
    }
    return path;
}

/* awg-quick creates the interface with "ip link add ... type amneziawg", for
 * which the kernel loads an installed amneziawg module by itself
 * (MODULE_ALIAS_RTNL_LINK), and falls back to amneziawg-go only when no module
 * can be loaded. A module that is not loaded yet still gets the parameters,
 * so load it the same way first and its version can be checked. Failing to
 * load one is not an error: awg-quick then uses amneziawg-go, and the tools
 * decide. */
static void
external_load_module(const gchar *version_file)
{
    g_autofree gchar *modprobe = NULL;
    gchar *argv[] = { NULL, (gchar *)"amneziawg", NULL };

    if (g_file_test(version_file, G_FILE_TEST_EXISTS))
        return;

    modprobe = find_system_program("modprobe");
    if (!modprobe)
        return;

    argv[0] = modprobe;
    g_spawn_sync(NULL, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                 NULL, NULL, NULL, NULL, NULL, NULL);
}

static gboolean
make_config_dir(AWGConnectionManagerExternalPrivate *priv, GError **error)
{
    g_autofree gchar *parent = g_path_get_dirname(priv->config_dir);

    /* Only the last component is private: a missing NetworkManager run-time
     * directory must stay readable for what NetworkManager puts there. */
    if (g_mkdir_with_parents(parent, 0755) != 0 || g_mkdir_with_parents(priv->config_dir, 0700) != 0) {
        int errsv = errno;

        g_set_error(error, AWG_CONNECTION_MANAGER_ERROR, 0,
                    "Failed to create %s: %s", priv->config_dir, g_strerror(errsv));
        return FALSE;
    }
    return TRUE;
}

/* Serializes the awg-quick runs on one interface name across service
 * instances: with concurrent connections a re-activation runs in a new
 * instance while the old one may still be taking that interface down.
 * Returns the descriptor holding the lock, or -1 to go on without one. */
static int
lock_interface(AWGConnectionManagerExternalPrivate *priv)
{
    g_autofree gchar *path = NULL;
    int fd;

    if (!make_config_dir(priv, NULL))
        return -1;

    path = g_strdup_printf("%s/%s.lock", priv->config_dir, priv->interface_name);
    fd = open(path, O_RDWR | O_CREAT | O_CLOEXEC, 0600);
    if (fd < 0)
        return -1;
    if (flock(fd, LOCK_EX) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void
unlock_interface(int fd)
{
    if (fd >= 0)
        close(fd);
}

/* Writes the configuration awg-quick reads, named after the interface (which
 * is how awg-quick picks the interface name) and readable by root only. */
static gboolean
write_config(AWGConnectionManagerExternalPrivate *priv, GError **error)
{
    if (!make_config_dir(priv, error))
        return FALSE;

    if (!awg_device_save_to_file(priv->device, priv->conf_path)) {
        g_set_error(error, AWG_CONNECTION_MANAGER_ERROR, 0,
                    "Failed to save config to %s", priv->conf_path);
        return FALSE;
    }

    if (chmod(priv->conf_path, 0400) != 0) {
        g_set_error(error, AWG_CONNECTION_MANAGER_ERROR, 0,
                    "Failed to chmod %s", priv->conf_path);
        unlink(priv->conf_path);
        return FALSE;
    }

    return TRUE;
}

static gboolean
run_awg_quick(const gchar *awg_quick, const gchar *command, const gchar *conf_path, GError **error)
{
    gchar *argv[] = { (gchar *)awg_quick, (gchar *)command, (gchar *)conf_path, NULL };
    gint exit_status = 0;

    if (!g_spawn_sync(NULL, argv, NULL, 0, NULL, NULL, NULL, NULL, &exit_status, error))
        return FALSE;

    /* Report a non-zero awg-quick exit (e.g. "Configuration parsing error"
     * from "awg setconf") instead of failing silently without a GError. */
    return g_spawn_check_exit_status(exit_status, error);
}

/* The nft tables awg-quick adds for a default route, named as in
 * add_default() of amneziawg-tools src/wg-quick/linux.bash */
static void
remove_nft_tables(const gchar *ifname)
{
    static const gchar *families[] = { "ip", "ip6" };
    g_autofree gchar *nft = find_system_program("nft");
    g_autofree gchar *table = g_strdup_printf("wg-quick-%s", ifname);

    if (!nft)
        return;

    for (guint i = 0; i < G_N_ELEMENTS(families); i++) {
        gchar *argv[] = { nft, (gchar *)"delete", (gchar *)"table", (gchar *)families[i], table, NULL };

        g_spawn_sync(NULL, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                     NULL, NULL, NULL, NULL, NULL, NULL);
    }
}

/* The name awg-quick registers its DNS under with resolvconf:
 * resolvconf_iface_prefix() in linux.bash prepends the first "<name>*" entry
 * of /etc/resolvconf/interface-order (Debian resolvconf) and a dot. */
static gchar *
resolvconf_record_name(const gchar *ifname)
{
    g_autofree gchar *contents = NULL;
    g_auto(GStrv) lines = NULL;

    if (!g_file_get_contents("/etc/resolvconf/interface-order", &contents, NULL, NULL))
        return g_strdup(ifname);

    lines = g_strsplit(contents, "\n", -1);
    for (guint i = 0; lines[i]; i++) {
        gchar *entry = g_strstrip(lines[i]);
        gsize len = strlen(entry);
        gsize name_len = 0;

        while (name_len < len && (g_ascii_isalnum(entry[name_len]) || entry[name_len] == '-'))
            name_len++;
        if (name_len > 0 && name_len == len - 1 && entry[name_len] == '*')
            return g_strdup_printf("%.*s.%s", (int)name_len, entry, ifname);
    }
    return g_strdup(ifname);
}

/* The DNS awg-quick set with resolvconf: without this, openresolv and Debian
 * resolvconf keep the unreachable servers ("unset_dns" in linux.bash). */
static void
remove_resolvconf_dns(const gchar *ifname)
{
    g_autofree gchar *resolvconf = find_system_program("resolvconf");
    g_autofree gchar *record = resolvconf_record_name(ifname);
    gchar *argv[] = { resolvconf, (gchar *)"-d", record, (gchar *)"-f", NULL };

    if (!resolvconf)
        return;

    g_spawn_sync(NULL, argv, NULL, G_SPAWN_STDOUT_TO_DEV_NULL | G_SPAWN_STDERR_TO_DEV_NULL,
                 NULL, NULL, NULL, NULL, NULL, NULL);
}

void
awg_connection_manager_external_remove_leftovers(const gchar *ifname)
{
    remove_nft_tables(ifname);
    remove_resolvconf_dns(ifname);
}

/* Takes @ifname down after @cause kept awg-quick from it. Its ip rules are inert
 * without the link; RTM_DELLINK also removes an amneziawg-go tun device. */
static gboolean
delete_link_fallback(const gchar *ifname, const GError *cause, GError **error)
{
    int ret;

    if (if_nametoindex(ifname) == 0) {
        g_message("Could not take down %s with awg-quick (%s); the interface is already gone",
                  ifname, cause->message);
    } else {
        ret = wg_del_device(ifname);
        if (ret != 0 && ret != -ENODEV) {
            g_set_error(error, AWG_CONNECTION_MANAGER_ERROR, 0,
                        "Could not take down %s with awg-quick (%s), and deleting the interface "
                        "failed too: %s",
                        ifname, cause->message, g_strerror(-ret));
            return FALSE;
        }
        g_warning("Could not take down %s with awg-quick (%s); deleted the interface directly",
                  ifname, cause->message);
    }

    /* Only once the link is gone: a tunnel that stays up keeps its rules */
    awg_connection_manager_external_remove_leftovers(ifname);
    return TRUE;
}

static gboolean
awg_connection_manager_external_connect(AWGConnectionManager *mgr, GCancellable *cancellable, GError **error)
{
    AWGConnectionManagerExternal *self = AWG_CONNECTION_MANAGER_EXTERNAL(mgr);
    AWGConnectionManagerExternalPrivate *priv = AWG_CONNECTION_MANAGER_EXTERNAL_GET_PRIVATE(self);
    const gchar *awg_quick = awg_quick_find_exe();
    gboolean success;
    int lock_fd;

    if (!awg_quick) {
        g_set_error(error, AWG_CONNECTION_MANAGER_ERROR, 0,
                    "awg-quick executable not found");
        return FALSE;
    }

    /* awg-quick creates the interface through the amneziawg module whenever
     * one can be loaded, so the module has to know the parameters as well,
     * not only "awg setconf". Without a module (amneziawg-go) there is no
     * version to check: the tools decide. */
    if (awg_device_get_required_awg_version(priv->device, NULL, NULL, NULL))
        external_load_module(priv->module_version_file);
    if (!awg_connection_manager_netlink_check_module(priv->device, priv->module_version_file, error))
        return FALSE;

    if (!external_check_awg31_support(priv->device, error))
        return FALSE;

    lock_fd = lock_interface(priv);
    if (!write_config(priv, error)) {
        unlock_interface(lock_fd);
        return FALSE;
    }

    /* The interface name comes from the connection, so an interface that
     * already has it is left over from an earlier activation whose teardown
     * never ran (the service was killed) or failed. "awg-quick up" would
     * refuse with "already exists": take it down first.
     * This assumes the name is unique per connection (awg-<first 8 UUID
     * chars>): concurrent connections must never share one. It may also be
     * the live link of an older instance of this connection still
     * disconnecting; the lock and the ifindex check there keep ours up. */
    if (if_nametoindex(priv->interface_name) != 0) {
        GError *down_error = NULL;

        g_message("%s is left over from an earlier activation, taking it down first",
                  priv->interface_name);
        if (!run_awg_quick(awg_quick, "down", priv->conf_path, &down_error)) {
            success = delete_link_fallback(priv->interface_name, down_error, error);
            g_clear_error(&down_error);
            if (!success) {
                unlink(priv->conf_path);
                unlock_interface(lock_fd);
                return FALSE;
            }
        } else {
            /* "down" unsets only DNS the new configuration has, so drop the
             * old record (and nft tables) here; both are idempotent */
            awg_connection_manager_external_remove_leftovers(priv->interface_name);
        }
    }

    success = run_awg_quick(awg_quick, "up", priv->conf_path, error);
    unlink(priv->conf_path);
    if (success)
        priv->ifindex = if_nametoindex(priv->interface_name);
    unlock_interface(lock_fd);
    return success;
}

static gboolean
awg_connection_manager_external_disconnect(AWGConnectionManager *mgr, GError **error)
{
    AWGConnectionManagerExternal *self = AWG_CONNECTION_MANAGER_EXTERNAL(mgr);
    AWGConnectionManagerExternalPrivate *priv = AWG_CONNECTION_MANAGER_EXTERNAL_GET_PRIVATE(self);
    const gchar *awg_quick = awg_quick_find_exe();
    GError *down_error = NULL;
    gboolean success;
    unsigned int current;
    int lock_fd = lock_interface(priv);

    /* NM may start the next activation of this profile in a new instance
     * before this Disconnect runs; a link with a new index is that one's. */
    current = if_nametoindex(priv->interface_name);
    if (priv->ifindex != 0 && current != 0 && current != priv->ifindex) {
        g_message("%s now belongs to a newer activation, leaving it up", priv->interface_name);
        unlock_interface(lock_fd);
        return TRUE;
    }

    /* awg-quick reads the hooks from the file again */
    if (!awg_quick)
        g_set_error(&down_error, AWG_CONNECTION_MANAGER_ERROR, 0, "awg-quick executable not found");
    else if (write_config(priv, &down_error))
        run_awg_quick(awg_quick, "down", priv->conf_path, &down_error);
    unlink(priv->conf_path);

    if (!down_error) {
        unlock_interface(lock_fd);
        return TRUE;
    }

    success = delete_link_fallback(priv->interface_name, down_error, error);
    g_error_free(down_error);
    unlock_interface(lock_fd);
    return success;
}

static gboolean
awg_connection_manager_external_manages_routes(AWGConnectionManager *mgr)
{
    return TRUE;
}

static void
awg_connection_manager_external_interface_init(AWGConnectionManagerInterface *iface)
{
    iface->connect = awg_connection_manager_external_connect;
    iface->disconnect = awg_connection_manager_external_disconnect;
    iface->manages_routes = awg_connection_manager_external_manages_routes;
}

static void
awg_connection_manager_external_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    AWGConnectionManagerExternal *self = AWG_CONNECTION_MANAGER_EXTERNAL(object);
    AWGConnectionManagerExternalPrivate *priv = AWG_CONNECTION_MANAGER_EXTERNAL_GET_PRIVATE(self);

    switch (prop_id) {
    case PROP_INTERFACE_NAME:
        priv->interface_name = g_value_dup_string(value);
        break;
    case PROP_DEVICE:
        if (priv->device)
            g_object_unref(priv->device);
        priv->device = g_value_dup_object(value);
        break;
    case PROP_CONFIG_DIR:
        g_free(priv->config_dir);
        priv->config_dir = g_value_dup_string(value);
        break;
    case PROP_MODULE_VERSION_FILE:
        g_free(priv->module_version_file);
        priv->module_version_file = g_value_dup_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
awg_connection_manager_external_constructed(GObject *object)
{
    AWGConnectionManagerExternal *self = AWG_CONNECTION_MANAGER_EXTERNAL(object);
    AWGConnectionManagerExternalPrivate *priv = AWG_CONNECTION_MANAGER_EXTERNAL_GET_PRIVATE(self);
    g_autofree gchar *interface_conf = g_strdup_printf("%s.conf", priv->interface_name);

    if (!priv->config_dir)
        priv->config_dir = g_strdup(AWG_EXTERNAL_CONFIG_DIR);
    if (!priv->module_version_file)
        priv->module_version_file = g_strdup(AMNEZIAWG_MODULE_VERSION_FILE);
    priv->conf_path = g_build_filename(priv->config_dir, interface_conf, NULL);

    G_OBJECT_CLASS(awg_connection_manager_external_parent_class)->constructed(object);
}

static void
awg_connection_manager_external_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    AWGConnectionManagerExternal *self = AWG_CONNECTION_MANAGER_EXTERNAL(object);
    AWGConnectionManagerExternalPrivate *priv = AWG_CONNECTION_MANAGER_EXTERNAL_GET_PRIVATE(self);

    switch (prop_id) {
    case PROP_INTERFACE_NAME:
        g_value_set_string(value, priv->interface_name);
        break;
    case PROP_DEVICE:
        g_value_set_object(value, priv->device);
        break;
    case PROP_CONFIG_DIR:
        g_value_set_string(value, priv->config_dir);
        break;
    case PROP_MODULE_VERSION_FILE:
        g_value_set_string(value, priv->module_version_file);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
awg_connection_manager_external_finalize(GObject *object)
{
    AWGConnectionManagerExternal *self = AWG_CONNECTION_MANAGER_EXTERNAL(object);
    AWGConnectionManagerExternalPrivate *priv = AWG_CONNECTION_MANAGER_EXTERNAL_GET_PRIVATE(self);

    g_free(priv->interface_name);
    g_free(priv->config_dir);
    g_free(priv->conf_path);
    g_free(priv->module_version_file);
    if (priv->device)
        g_object_unref(priv->device);

    G_OBJECT_CLASS(awg_connection_manager_external_parent_class)->finalize(object);
}

static void
awg_connection_manager_external_class_init(AWGConnectionManagerExternalClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = awg_connection_manager_external_set_property;
    object_class->get_property = awg_connection_manager_external_get_property;
    object_class->constructed = awg_connection_manager_external_constructed;
    object_class->finalize = awg_connection_manager_external_finalize;

    g_object_class_install_property(object_class, PROP_INTERFACE_NAME,
                                    g_param_spec_string("interface-name", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(object_class, PROP_DEVICE,
                                    g_param_spec_object("device", NULL, NULL,
                                                        AWG_TYPE_DEVICE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    /* Directory for the generated configuration, AWG_EXTERNAL_CONFIG_DIR
     * unless set (the tests run unprivileged). */
    g_object_class_install_property(object_class, PROP_CONFIG_DIR,
                                    g_param_spec_string("config-dir", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    /* Where the loaded amneziawg module reports its version,
     * AMNEZIAWG_MODULE_VERSION_FILE unless set (the tests must not depend on
     * the module of the host). */
    g_object_class_install_property(object_class, PROP_MODULE_VERSION_FILE,
                                    g_param_spec_string("module-version-file", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));
}

static void
awg_connection_manager_external_init(AWGConnectionManagerExternal *self)
{
}

AWGConnectionManager *
awg_connection_manager_external_new(const gchar *interface_name, AWGDevice *device)
{
    return g_object_new(AWG_TYPE_CONNECTION_MANAGER_EXTERNAL,
                        "interface-name", interface_name,
                        "device", device,
                        NULL);
}
