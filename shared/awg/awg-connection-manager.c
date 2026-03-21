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

#include "awg-connection-manager.h"
#include "awg-connection-manager-dummy.h"
#include "awg-connection-manager-external.h"
#include "awg-connection-manager-netlink.h"

G_DEFINE_INTERFACE(AWGConnectionManager, awg_connection_manager, G_TYPE_OBJECT)

static void
awg_connection_manager_default_init(AWGConnectionManagerInterface *iface)
{
}

AWGConnectionManager *
awg_connection_manager_auto_new(const gchar *interface_name, AWGDevice *device)
{
    const gchar *force_awg_quick = g_getenv("NM_FORCE_AWG_QUICK");

    if (force_awg_quick && *force_awg_quick) {
        if (awg_connection_manager_external_is_available())
            return awg_connection_manager_external_new(interface_name, device);
    }

    if (awg_connection_manager_netlink_is_available())
        return awg_connection_manager_netlink_new(interface_name, device);

    if (awg_connection_manager_external_is_available())
        return awg_connection_manager_external_new(interface_name, device);

    return awg_connection_manager_dummy_new(interface_name, device);
}

gboolean
awg_connection_manager_connect(AWGConnectionManager *self, GError **error)
{
    AWGConnectionManagerInterface *iface;

    g_return_val_if_fail(AWG_IS_CONNECTION_MANAGER(self), FALSE);

    iface = AWG_CONNECTION_MANAGER_GET_IFACE(self);
    if (iface->connect)
        return iface->connect(self, error);

    return FALSE;
}

gboolean
awg_connection_manager_disconnect(AWGConnectionManager *self, GError **error)
{
    AWGConnectionManagerInterface *iface;

    g_return_val_if_fail(AWG_IS_CONNECTION_MANAGER(self), FALSE);

    iface = AWG_CONNECTION_MANAGER_GET_IFACE(self);
    if (iface->disconnect)
        return iface->disconnect(self, error);

    return TRUE;
}

gboolean
awg_connection_manager_manages_routes(AWGConnectionManager *self)
{
    AWGConnectionManagerInterface *iface;

    g_return_val_if_fail(AWG_IS_CONNECTION_MANAGER(self), FALSE);

    iface = AWG_CONNECTION_MANAGER_GET_IFACE(self);
    if (iface->manages_routes)
        return iface->manages_routes(self);

    return FALSE;
}