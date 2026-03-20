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

#ifndef AWG_DEVICE_H
#define AWG_DEVICE_H

#include <NetworkManager.h>
#include <gio/gio.h>
#include <glib.h>

#define AWG_KEY_SIZE 32

typedef struct _AWGSubNet AWGSubNet;
typedef struct _AWGDevicePeer AWGDevicePeer;
typedef struct _AWGDevice AWGDevice;

GType awg_subnet_get_type(void);
#define AWG_TYPE_SUBNET (awg_subnet_get_type())
#define AWG_SUBNET(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), AWG_TYPE_SUBNET, AWGSubNet))
#define AWG_SUBNET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), AWG_TYPE_SUBNET, AWGSubnetClass))
#define AWG_IS_SUBNET(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), AWG_TYPE_SUBNET))
#define AWG_IS_SUBNET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), AWG_TYPE_SUBNET))

GType awg_device_get_type(void);
#define AWG_TYPE_DEVICE (awg_device_get_type())
#define AWG_DEVICE(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), AWG_TYPE_DEVICE, AWGDevice))
#define AWG_DEVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), AWG_TYPE_DEVICE, AWGDeviceClass))
#define AWG_IS_DEVICE(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), AWG_TYPE_DEVICE))
#define AWG_IS_DEVICE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), AWG_TYPE_DEVICE))

GType awg_device_peer_get_type(void);
#define AWG_TYPE_DEVICE_PEER (awg_device_peer_get_type())
#define AWG_DEVICE_PEER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), AWG_TYPE_DEVICE_PEER, AWGDevicePeer))
#define AWG_DEVICE_PEER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), AWG_TYPE_DEVICE_PEER, AWGDevicePeerClass))
#define AWG_IS_DEVICE_PEER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), AWG_TYPE_DEVICE_PEER))
#define AWG_IS_DEVICE_PEER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), AWG_TYPE_DEVICE_PEER))

AWGSubNet *awg_subnet_new(GInetAddress *address, guint32 mask);
void awg_subnet_free(AWGSubNet *subnet);
AWGSubNet *awg_subnet_new_from_string(const char *str);
char *awg_subnet_to_string(const AWGSubNet *subnet);

AWGDevice *awg_device_new(void);

const gchar *awg_device_get_private_key(AWGDevice *self);
const gchar *awg_device_get_public_key(AWGDevice *self);
NMSettingSecretFlags awg_device_get_private_key_flags(AWGDevice *self);
void awg_device_set_private_key_flags(AWGDevice *self, NMSettingSecretFlags flags);
guint32 awg_device_get_fw_mark(AWGDevice *self);
guint16 awg_device_get_listen_port(AWGDevice *self);
const GInetAddress *awg_device_get_address_v4(AWGDevice *self);
const GInetAddress *awg_device_get_address_v6(AWGDevice *self);
const GList *awg_device_get_dns_v4_list(AWGDevice *self);
guint awg_device_get_dns_v4_count(AWGDevice *self);
const GList *awg_device_get_dns_v6_list(AWGDevice *self);
guint awg_device_get_dns_v6_count(AWGDevice *self);
guint16 awg_device_get_jc(AWGDevice *self);
guint16 awg_device_get_jmin(AWGDevice *self);
guint16 awg_device_get_jmax(AWGDevice *self);
guint16 awg_device_get_s1(AWGDevice *self);
guint16 awg_device_get_s2(AWGDevice *self);
guint16 awg_device_get_s3(AWGDevice *self);
guint16 awg_device_get_s4(AWGDevice *self);
const gchar *awg_device_get_h1(AWGDevice *self);
const gchar *awg_device_get_h2(AWGDevice *self);
const gchar *awg_device_get_h3(AWGDevice *self);
const gchar *awg_device_get_h4(AWGDevice *self);
const gchar *awg_device_get_i1(AWGDevice *self);
const gchar *awg_device_get_i2(AWGDevice *self);
const gchar *awg_device_get_i3(AWGDevice *self);
const gchar *awg_device_get_i4(AWGDevice *self);
const gchar *awg_device_get_i5(AWGDevice *self);

gboolean awg_device_set_private_key(AWGDevice *self, const gchar *primary_key);
gboolean awg_device_set_public_key(AWGDevice *self, const gchar *public_key);
gboolean awg_device_set_fw_mark(AWGDevice *self, guint64 fw_mark);
gboolean awg_device_set_fw_mark_from_string(AWGDevice *self, const gchar *str);
gboolean awg_device_set_address_from_string(AWGDevice *self, const gchar *str);
gboolean awg_device_set_listen_port(AWGDevice *self, guint64 port);
gboolean awg_device_set_listen_port_from_string(AWGDevice *self, const gchar *str);
gboolean awg_device_set_dns_from_string(AWGDevice *self, const gchar *str);
gboolean awg_device_set_jc(AWGDevice *self, guint64 jc);
gboolean awg_device_set_jc_from_string(AWGDevice *self, const gchar *str);
gboolean awg_device_set_jmin(AWGDevice *self, guint64 jmin);
gboolean awg_device_set_jmin_from_string(AWGDevice *self, const gchar *str);
gboolean awg_device_set_jmax(AWGDevice *self, guint64 jmax);
gboolean awg_device_set_jmax_from_string(AWGDevice *self, const gchar *str);
gboolean awg_device_set_s1(AWGDevice *self, guint64 s1);
gboolean awg_device_set_s1_from_string(AWGDevice *self, const gchar *str);
gboolean awg_device_set_s2(AWGDevice *self, guint64 s2);
gboolean awg_device_set_s2_from_string(AWGDevice *self, const gchar *str);
gboolean awg_device_set_s3(AWGDevice *self, guint64 s3);
gboolean awg_device_set_s3_from_string(AWGDevice *self, const gchar *str);
gboolean awg_device_set_s4(AWGDevice *self, guint64 s4);
gboolean awg_device_set_s4_from_string(AWGDevice *self, const gchar *str);
gboolean awg_device_set_h1(AWGDevice *self, const gchar *h1);
gboolean awg_device_set_h2(AWGDevice *self, const gchar *h2);
gboolean awg_device_set_h3(AWGDevice *self, const gchar *h3);
gboolean awg_device_set_h4(AWGDevice *self, const gchar *h4);
gboolean awg_device_set_i1(AWGDevice *self, const gchar *i1);
gboolean awg_device_set_i2(AWGDevice *self, const gchar *i2);
gboolean awg_device_set_i3(AWGDevice *self, const gchar *i3);
gboolean awg_device_set_i4(AWGDevice *self, const gchar *i4);
gboolean awg_device_set_i5(AWGDevice *self, const gchar *i5);

guint32 awg_device_get_mtu(AWGDevice *self);
gboolean awg_device_set_mtu(AWGDevice *self, guint32 mtu);
gboolean awg_device_set_mtu_from_string(AWGDevice *self, const gchar *str);

gboolean awg_device_set_pre_up(AWGDevice *self, const gchar *script);
const gchar *awg_device_get_pre_up(AWGDevice *self);
gboolean awg_device_set_post_up(AWGDevice *self, const gchar *script);
const gchar *awg_device_get_post_up(AWGDevice *self);
gboolean awg_device_set_pre_down(AWGDevice *self, const gchar *script);
const gchar *awg_device_get_pre_down(AWGDevice *self);
gboolean awg_device_set_post_down(AWGDevice *self, const gchar *script);
const gchar *awg_device_get_post_down(AWGDevice *self);

gboolean awg_device_add_peer(AWGDevice *self, AWGDevicePeer *peer);
gboolean awg_device_remove_peer(AWGDevice *self, guint index);
const GList *awg_device_get_peers_list(AWGDevice *self);
guint awg_device_get_peers_count(AWGDevice *self);
gboolean awg_device_is_valid(AWGDevice *self);
void awg_device_free(AWGDevice *device);

AWGDevicePeer *awg_device_peer_new(void);
void awg_device_peer_free(AWGDevicePeer *peer);
gboolean awg_device_peer_is_valid(AWGDevicePeer *self);

const gchar *awg_device_peer_get_public_key(AWGDevicePeer *self);
const gchar *awg_device_peer_get_shared_key(AWGDevicePeer *self);
NMSettingSecretFlags awg_device_peer_get_shared_key_flags(AWGDevicePeer *self);
void awg_device_peer_set_shared_key_flags(AWGDevicePeer *self, NMSettingSecretFlags flags);
gchar *awg_device_peer_get_allowed_ips_as_string(AWGDevicePeer *self);
const gchar *awg_device_peer_get_endpoint(AWGDevicePeer *self);
guint16 awg_device_peer_get_keep_alive_interval(AWGDevicePeer *self);
gboolean awg_device_peer_get_advanced_security(AWGDevicePeer *self);

gboolean awg_device_peer_set_public_key(AWGDevicePeer *self, const gchar *public_key);
gboolean awg_device_peer_set_shared_key(AWGDevicePeer *self, const gchar *shared_key);
gboolean awg_device_peer_set_allowed_ips_from_string(AWGDevicePeer *self, const gchar *allowed_ips);
gboolean awg_device_peer_set_endpoint(AWGDevicePeer *self, const gchar *endpoint);
gboolean awg_device_peer_set_keep_alive_interval(AWGDevicePeer *self, guint64 interval);
gboolean awg_device_peer_set_keep_alive_interval_from_string(AWGDevicePeer *self, const gchar *str);
void awg_device_peer_set_advanced_security(AWGDevicePeer *self, gboolean enabled);

#endif // AWG_DEVICE_H
