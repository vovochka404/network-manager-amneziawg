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

#ifndef AWG_CONNECTION_MANAGER_H
#define AWG_CONNECTION_MANAGER_H

#include "awg-device.h"
#include <gio/gio.h>
#include <glib-object.h>

G_BEGIN_DECLS

GType awg_connection_manager_get_type(void) G_GNUC_CONST;
#define AWG_TYPE_CONNECTION_MANAGER (awg_connection_manager_get_type())
#define AWG_CONNECTION_MANAGER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), AWG_TYPE_CONNECTION_MANAGER, AWGConnectionManager))
#define AWG_CONNECTION_MANAGER_GET_IFACE(obj) \
    (G_TYPE_INSTANCE_GET_INTERFACE((obj), AWG_TYPE_CONNECTION_MANAGER, AWGConnectionManagerInterface))
#define AWG_IS_CONNECTION_MANAGER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), AWG_TYPE_CONNECTION_MANAGER))

typedef struct _AWGConnectionManager AWGConnectionManager;
typedef struct _AWGConnectionManagerInterface AWGConnectionManagerInterface;

struct _AWGConnectionManagerInterface {
    GTypeInterface parent_iface;

    gboolean (*connect)(AWGConnectionManager *self, GError **error);
    gboolean (*disconnect)(AWGConnectionManager *self, GError **error);
};

AWGConnectionManager *awg_connection_manager_auto_new(const gchar *interface_name, AWGDevice *device);

gboolean awg_connection_manager_connect(AWGConnectionManager *self, GError **error);
gboolean awg_connection_manager_disconnect(AWGConnectionManager *self, GError **error);

G_END_DECLS

#endif /* AWG_CONNECTION_MANAGER_H */