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

#include "awg-connection-manager-dummy.h"
#include <glib-object.h>
#include <glib/gi18n.h>

typedef struct _AWGConnectionManagerDummyPrivate AWGConnectionManagerDummyPrivate;

struct _AWGConnectionManagerDummy {
    GObject parent_instance;
};

struct _AWGConnectionManagerDummyClass {
    GObjectClass parent_class;
};

struct _AWGConnectionManagerDummyPrivate {
    gchar *interface_name;
    AWGDevice *device;
    gboolean running;
};

enum {
    PROP_0,
    PROP_INTERFACE_NAME,
    PROP_DEVICE,
    N_PROPS
};

static void awg_connection_manager_dummy_interface_init(AWGConnectionManagerInterface *iface);

G_DEFINE_TYPE_WITH_CODE(AWGConnectionManagerDummy, awg_connection_manager_dummy, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(AWG_TYPE_CONNECTION_MANAGER, awg_connection_manager_dummy_interface_init) G_ADD_PRIVATE(AWGConnectionManagerDummy))

#define AWG_CONNECTION_MANAGER_DUMMY_ERROR g_quark_from_static_string("awg-connection-manager-dummy-error-quark")

#define AWG_CONNECTION_MANAGER_DUMMY_GET_PRIVATE(self) \
    ((AWGConnectionManagerDummyPrivate *)awg_connection_manager_dummy_get_instance_private(self))

static gboolean
awg_connection_manager_dummy_connect(AWGConnectionManager *mgr, GError **error)
{
    g_set_error(error, AWG_CONNECTION_MANAGER_DUMMY_ERROR, 1,
                _("Neither amneziawg kernel module nor awg-quick is available"));
    return FALSE;
}

static gboolean
awg_connection_manager_dummy_disconnect(AWGConnectionManager *mgr, GError **error)
{
    g_set_error(error, AWG_CONNECTION_MANAGER_DUMMY_ERROR, 1,
                _("Neither amneziawg kernel module nor awg-quick is available"));
    return FALSE;
}

static gboolean
awg_connection_manager_dummy_manages_routes(AWGConnectionManager *mgr)
{
    return FALSE;
}

static void
awg_connection_manager_dummy_interface_init(AWGConnectionManagerInterface *iface)
{
    iface->connect = awg_connection_manager_dummy_connect;
    iface->disconnect = awg_connection_manager_dummy_disconnect;
    iface->manages_routes = awg_connection_manager_dummy_manages_routes;
}

static void
awg_connection_manager_dummy_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    AWGConnectionManagerDummy *self = AWG_CONNECTION_MANAGER_DUMMY(object);
    AWGConnectionManagerDummyPrivate *priv = AWG_CONNECTION_MANAGER_DUMMY_GET_PRIVATE(self);

    switch (prop_id) {
    case PROP_INTERFACE_NAME:
        priv->interface_name = g_value_dup_string(value);
        break;
    case PROP_DEVICE:
        priv->device = g_value_dup_object(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
    }
}

static void
awg_connection_manager_dummy_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    AWGConnectionManagerDummy *self = AWG_CONNECTION_MANAGER_DUMMY(object);
    AWGConnectionManagerDummyPrivate *priv = AWG_CONNECTION_MANAGER_DUMMY_GET_PRIVATE(self);

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
awg_connection_manager_dummy_finalize(GObject *object)
{
    AWGConnectionManagerDummy *self = AWG_CONNECTION_MANAGER_DUMMY(object);
    AWGConnectionManagerDummyPrivate *priv = AWG_CONNECTION_MANAGER_DUMMY_GET_PRIVATE(self);

    g_free(priv->interface_name);
    if (priv->device)
        g_object_unref(priv->device);

    G_OBJECT_CLASS(awg_connection_manager_dummy_parent_class)->finalize(object);
}

static void
awg_connection_manager_dummy_class_init(AWGConnectionManagerDummyClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->set_property = awg_connection_manager_dummy_set_property;
    object_class->get_property = awg_connection_manager_dummy_get_property;
    object_class->finalize = awg_connection_manager_dummy_finalize;

    g_object_class_install_property(object_class, PROP_INTERFACE_NAME,
                                    g_param_spec_string("interface-name", NULL, NULL,
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

    g_object_class_install_property(object_class, PROP_DEVICE,
                                    g_param_spec_object("device", NULL, NULL,
                                                        AWG_TYPE_DEVICE,
                                                        G_PARAM_CONSTRUCT_ONLY | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));
}

static void
awg_connection_manager_dummy_init(AWGConnectionManagerDummy *self)
{
}

AWGConnectionManager *
awg_connection_manager_dummy_new(const gchar *interface_name, AWGDevice *device)
{
    return g_object_new(AWG_TYPE_CONNECTION_MANAGER_DUMMY,
                        "interface-name", interface_name,
                        "device", device,
                        NULL);
}

gboolean
awg_connection_manager_dummy_is_available(void)
{
    return TRUE;
}
