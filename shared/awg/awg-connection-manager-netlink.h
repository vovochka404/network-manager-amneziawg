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

/* Refuses @device when it uses an AmneziaWG 3.x parameter the loaded amneziawg
 * kernel module does not know, reading the module release from @version_file
 * (AMNEZIAWG_MODULE_VERSION_FILE when NULL). A version that cannot be read (no
 * module loaded) or parsed is not refused: the kernel then decides. Applies to
 * the awg-quick backend as well, which uses the same module. */
gboolean awg_connection_manager_netlink_check_module(AWGDevice *device, const gchar *version_file, GError **error);

gboolean awg_connection_manager_netlink_add_routes(AWGConnectionManager *mgr, int family, GError **error);
gboolean awg_connection_manager_netlink_delete_routes(AWGConnectionManager *mgr, int family, GError **error);

G_END_DECLS

#endif /* AWG_CONNECTION_MANAGER_NETLINK_H */
