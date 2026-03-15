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

#ifndef AWG_CONNECTION_MANAGER_DUMMY_H
#define AWG_CONNECTION_MANAGER_DUMMY_H

#include "awg-connection-manager.h"

G_BEGIN_DECLS

typedef struct _AWGConnectionManagerDummy AWGConnectionManagerDummy;
typedef struct _AWGConnectionManagerDummyClass AWGConnectionManagerDummyClass;

GType awg_connection_manager_dummy_get_type(void) G_GNUC_CONST;
#define AWG_TYPE_CONNECTION_MANAGER_DUMMY (awg_connection_manager_dummy_get_type())
#define AWG_CONNECTION_MANAGER_DUMMY(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), AWG_TYPE_CONNECTION_MANAGER_DUMMY, AWGConnectionManagerDummy))
#define AWG_CONNECTION_MANAGER_DUMMY_CLASS(obj) \
    (G_TYPE_CHECK_CLASS_CAST((obj), AWG_TYPE_CONNECTION_MANAGER_DUMMY, AWGConnectionManagerDummyClass))
#define AWG_IS_CONNECTION_MANAGER_DUMMY(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), AWG_TYPE_CONNECTION_MANAGER_DUMMY))

gboolean awg_connection_manager_dummy_is_available(void);

AWGConnectionManager *awg_connection_manager_dummy_new(const gchar *interface_name, AWGDevice *device);

G_END_DECLS

#endif /* AWG_CONNECTION_MANAGER_DUMMY_H */
