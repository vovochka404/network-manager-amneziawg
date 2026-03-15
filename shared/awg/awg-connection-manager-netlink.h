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

#ifndef AWG_CONNECTION_MANAGER_NETLINK_H
#define AWG_CONNECTION_MANAGER_NETLINK_H

#include "awg-connection-manager.h"

G_BEGIN_DECLS

typedef struct _AWGConnectionManagerNetlink AWGConnectionManagerNetlink;
typedef struct _AWGConnectionManagerNetlinkClass AWGConnectionManagerNetlinkClass;

GType awg_connection_manager_netlink_get_type(void) G_GNUC_CONST;
#define AWG_TYPE_CONNECTION_MANAGER_NETLINK (awg_connection_manager_netlink_get_type())
#define AWG_CONNECTION_MANAGER_NETLINK(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), AWG_TYPE_CONNECTION_MANAGER_NETLINK, AWGConnectionManagerNetlink))
#define AWG_CONNECTION_MANAGER_NETLINK_CLASS(obj) \
    (G_TYPE_CHECK_CLASS_CAST((obj), AWG_TYPE_CONNECTION_MANAGER_NETLINK, AWGConnectionManagerNetlinkClass))
#define AWG_IS_CONNECTION_MANAGER_NETLINK(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), AWG_TYPE_CONNECTION_MANAGER_NETLINK))

gboolean awg_connection_manager_netlink_is_available(void);

AWGConnectionManager *awg_connection_manager_netlink_new(const gchar *interface_name, AWGDevice *device);

G_END_DECLS

#endif /* AWG_CONNECTION_MANAGER_NETLINK_H */
