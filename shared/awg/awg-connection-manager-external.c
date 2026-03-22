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
#include "awg-config.h"
#include <glib.h>
#include <sys/stat.h>
#include <unistd.h>

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
    gchar *conf_path;
};

enum {
    PROP_0,
    PROP_INTERFACE_NAME,
    PROP_DEVICE,
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

static gboolean
awg_connection_manager_external_connect(AWGConnectionManager *mgr, GError **error)
{
    AWGConnectionManagerExternal *self = AWG_CONNECTION_MANAGER_EXTERNAL(mgr);
    AWGConnectionManagerExternalPrivate *priv = AWG_CONNECTION_MANAGER_EXTERNAL_GET_PRIVATE(self);
    const gchar *awg_quick = awg_quick_find_exe();
    gchar *command = NULL;
    gint exit_status = 0;
    gboolean success = FALSE;

    if (!awg_quick) {
        g_set_error(error, AWG_CONNECTION_MANAGER_ERROR, 0,
                    "awg-quick executable not found");
        return FALSE;
    }

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

    command = g_strdup_printf("%s up '%s'", awg_quick, priv->conf_path);
    success = g_spawn_command_line_sync(command, NULL, NULL, &exit_status, error);

    unlink(priv->conf_path);
    g_free(command);

    return success && exit_status == 0;
}

static gboolean
awg_connection_manager_external_disconnect(AWGConnectionManager *mgr, GError **error)
{
    AWGConnectionManagerExternal *self = AWG_CONNECTION_MANAGER_EXTERNAL(mgr);
    AWGConnectionManagerExternalPrivate *priv = AWG_CONNECTION_MANAGER_EXTERNAL_GET_PRIVATE(self);
    const gchar *awg_quick = awg_quick_find_exe();
    gchar *command = NULL;
    gint exit_status = 0;
    gboolean success = FALSE;

    if (!awg_quick) {
        g_set_error(error, AWG_CONNECTION_MANAGER_ERROR, 0,
                    "awg-quick executable not found");
        return FALSE;
    }

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

    command = g_strdup_printf("%s down '%s'", awg_quick, priv->conf_path);
    success = g_spawn_command_line_sync(command, NULL, NULL, &exit_status, error);

    unlink(priv->conf_path);
    g_free(command);

    return success && exit_status == 0;
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
    g_autofree gchar *interface_conf = NULL;

    switch (prop_id) {
    case PROP_INTERFACE_NAME:
        priv->interface_name = g_value_dup_string(value);
        interface_conf = g_strdup_printf("%s.conf", priv->interface_name);
        priv->conf_path = g_build_filename(g_get_tmp_dir(), interface_conf, NULL);
        break;
    case PROP_DEVICE:
        if (priv->device)
            g_object_unref(priv->device);
        priv->device = g_value_dup_object(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
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
    g_free(priv->conf_path);
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
    object_class->finalize = awg_connection_manager_external_finalize;

    g_object_class_install_property(object_class, PROP_INTERFACE_NAME,
                                    g_param_spec_string("interface-name", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(object_class, PROP_DEVICE,
                                    g_param_spec_object("device", NULL, NULL,
                                                        AWG_TYPE_DEVICE,
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
