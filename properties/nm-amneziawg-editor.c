/* -*- Mode: C; tab-width: 4; indent-tabs-mode: t; c-basic-offset: 4 -*- */
/***************************************************************************
 * CVSID: $Id: nm-openvpn.c 4232 2008-10-29 09:13:40Z tambeti $
 *
 * nm-openvpn.c : GNOME UI dialogs for configuring openvpn VPN connections
 *
 * Copyright (C) 2005 Tim Niemueller <tim@niemueller.de>
 * Copyright (C) 2008 - 2010 Dan Williams, <dcbw@redhat.com>
 * Copyright (C) 2008 - 2011 Red Hat, Inc.
 * Based on work by David Zeuthen, <davidz@redhat.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 *
 **************************************************************************/

#include "nm-default.h"

#include "gtk-compat.h"
#include "nm-amneziawg-editor.h"
#include <nma-ui-utils.h>

#include <arpa/inet.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>

#include "awg/awg-config.h"
#include "awg/awg-device.h"
#include "awg/awg-nm-connection.h"
#include "awg/awg-validate.h"

/*****************************************************************************/

static void amneziawg_editor_plugin_widget_interface_init(NMVpnEditorInterface *iface_class);

G_DEFINE_TYPE_EXTENDED(AmneziaWGEditor, amneziawg_editor_plugin_widget, G_TYPE_OBJECT, 0, G_IMPLEMENT_INTERFACE(NM_TYPE_VPN_EDITOR, amneziawg_editor_plugin_widget_interface_init))

#define AMNEZIAWG_EDITOR_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE((o), AMNEZIAWG_TYPE_EDITOR, AmneziaWGEditorPrivate))

typedef struct {
    GtkBuilder *builder;
    GtkWidget *widget;
    gboolean new_connection;
    NMConnection *connection;
    AWGDevice *device;
    guint selected_peer_index;
} AmneziaWGEditorPrivate;

/*****************************************************************************/
// functions for checking the contents of the input fields in the GUI

static gboolean
check_interface_mtu_entry(const char *str)
{
    return awg_validate_mtu(str);
}

static gboolean
check_interface_fwmark_entry(const char *str)
{
    return awg_validate_fw_mark(str);
}

static gboolean
check_interface_junk_count_entry(const char *str)
{
    return awg_validate_jc(str);
}

static gboolean
check_interface_junk_size_entry(const char *str)
{
    return awg_validate_junk_size(str);
}

static gboolean
check_interface_i_packet(const char *str)
{
    return awg_validate_i_packet(str);
}

static gboolean
check_interface_s3(const char *str)
{
    return awg_validate_s3(str);
}

static gboolean
check_interface_s4(const char *str)
{
    return awg_validate_s4(str);
}

static gboolean
check_interface_magic_header(const char *str)
{
    return awg_validate_magic_header(str);
}

static gboolean
check_interface_private_key(const char *str)
{
    return awg_validate_base64(str) && str && str[0];
}

static gboolean
check_interface_listen_port(const char *str)
{
    return awg_validate_port(str);
}

static gboolean
check_peer_public_key(const char *str)
{
    return awg_validate_base64(str) && str && str[0];
}

static gboolean
check_peer_endpoint(const char *str)
{
    return awg_validate_endpoint(str) && str && str[0];
}

static gboolean
check_peer_allowed_ips(const char *str)
{
    return awg_validate_allowed_ips(str);
}

// used in 'check()', matches the functions above
typedef gboolean (*CheckFunc)(const char *str);

// helper function to reduce boilerplate code in 'check_validity()'
static gboolean
check(const AmneziaWGEditorPrivate *priv,
      const char *widget_name,
      CheckFunc chk,
      const char *key,
      gboolean set_error,
      GError **error)
{
    GtkWidget *widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, widget_name));
    const char *str = AWG_EDITABLE_GET_TEXT(GTK_ENTRY(widget));
    if (str && chk(str))
        gtk_style_context_remove_class(gtk_widget_get_style_context(widget), "error");
    else {
        gtk_style_context_add_class(gtk_widget_get_style_context(widget), "error");
        // only set the error if it's NULL
        if (error == NULL && set_error) {
            g_set_error(error,
                        NMV_EDITOR_PLUGIN_ERROR,
                        NMV_EDITOR_PLUGIN_ERROR_INVALID_PROPERTY,
                        "%s",
                        key);
        }
        return FALSE;
    }

    return TRUE;
}

// add or remove the "error" class from the specified input field
// check validity of the input fields in the GUI
// if there is an error in one or more of the input fields, mark the corresponding
// input fields with the "error" class
static gboolean
check_validity(AmneziaWGEditor *self, GError **error)
{
    AmneziaWGEditorPrivate *priv = AMNEZIAWG_EDITOR_GET_PRIVATE(self);
    gboolean success = TRUE;
    NMSettingVpn *s_vpn = nm_connection_get_setting_vpn(priv->connection);
    NMSettingSecretFlags flags = NM_SETTING_SECRET_FLAG_NONE;

    nm_setting_get_secret_flags(NM_SETTING(s_vpn), NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY, &flags, NULL);

    if (!(flags & (NM_SETTING_SECRET_FLAG_NOT_REQUIRED | NM_SETTING_SECRET_FLAG_NOT_SAVED))) {
        if (!check(priv, "interface_private_key_entry", check_interface_private_key, AWG_CONFIG_DEVICE_PRIVATE_KEY, TRUE, error)) {
            success = FALSE;
        }
    } else {
        GtkWidget *widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_private_key_entry"));
        gtk_style_context_remove_class(gtk_widget_get_style_context(widget), "error");
    }
    if (!check(priv, "interface_port_entry", check_interface_listen_port, AWG_CONFIG_DEVICE_LISTEN_PORT, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_mtu_entry", check_interface_mtu_entry, "MTU", FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_fwmark_entry", check_interface_fwmark_entry, "FwMark", FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_jc_entry", check_interface_junk_count_entry, AWG_CONFIG_DEVICE_JC, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_jmin_entry", check_interface_junk_size_entry, AWG_CONFIG_DEVICE_JMIN, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_jmax_entry", check_interface_junk_size_entry, AWG_CONFIG_DEVICE_JMAX, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_s1_entry", check_interface_junk_size_entry, AWG_CONFIG_DEVICE_S1, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_s2_entry", check_interface_junk_size_entry, AWG_CONFIG_DEVICE_S2, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_s3_entry", check_interface_s3, AWG_CONFIG_DEVICE_S3, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_s4_entry", check_interface_s4, AWG_CONFIG_DEVICE_S4, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_h1_entry", check_interface_magic_header, AWG_CONFIG_DEVICE_H1, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_h2_entry", check_interface_magic_header, AWG_CONFIG_DEVICE_H2, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_h3_entry", check_interface_magic_header, AWG_CONFIG_DEVICE_H3, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_h4_entry", check_interface_magic_header, AWG_CONFIG_DEVICE_H4, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_i1_entry", check_interface_i_packet, AWG_CONFIG_DEVICE_I1, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_i2_entry", check_interface_i_packet, AWG_CONFIG_DEVICE_I2, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_i3_entry", check_interface_i_packet, AWG_CONFIG_DEVICE_I3, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_i4_entry", check_interface_i_packet, AWG_CONFIG_DEVICE_I4, FALSE, error)) {
        success = FALSE;
    }
    if (!check(priv, "interface_i5_entry", check_interface_i_packet, AWG_CONFIG_DEVICE_I5, FALSE, error)) {
        success = FALSE;
    }

    if (!awg_validate_magic_headers_no_overlap(
            AWG_EDITABLE_GET_TEXT(gtk_builder_get_object(priv->builder, "interface_h1_entry")),
            AWG_EDITABLE_GET_TEXT(gtk_builder_get_object(priv->builder, "interface_h2_entry")),
            AWG_EDITABLE_GET_TEXT(gtk_builder_get_object(priv->builder, "interface_h3_entry")),
            AWG_EDITABLE_GET_TEXT(gtk_builder_get_object(priv->builder, "interface_h4_entry")))) {
        GtkWidget *w1 = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_h1_entry"));
        GtkWidget *w2 = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_h2_entry"));
        GtkWidget *w3 = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_h3_entry"));
        GtkWidget *w4 = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_h4_entry"));
        gtk_style_context_add_class(gtk_widget_get_style_context(w1), "error");
        gtk_style_context_add_class(gtk_widget_get_style_context(w2), "error");
        gtk_style_context_add_class(gtk_widget_get_style_context(w3), "error");
        gtk_style_context_add_class(gtk_widget_get_style_context(w4), "error");
        if (error && !*error) {
            g_set_error_literal(error, NMV_EDITOR_PLUGIN_ERROR, 0, _("H1-H4 magic header ranges must not overlap"));
        }
        success = FALSE;
    }

    guint16 jmin = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(priv->builder, "interface_jmin_entry")));
    guint16 jmax = gtk_spin_button_get_value(GTK_SPIN_BUTTON(gtk_builder_get_object(priv->builder, "interface_jmax_entry")));
    if (!awg_validate_jmin_jmax(jmin, jmax)) {
        GtkWidget *w1 = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_jmin_entry"));
        GtkWidget *w2 = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_jmax_entry"));
        gtk_style_context_add_class(gtk_widget_get_style_context(w1), "error");
        gtk_style_context_add_class(gtk_widget_get_style_context(w2), "error");
        if (error && !*error) {
            g_set_error_literal(error, NMV_EDITOR_PLUGIN_ERROR, 0, _("JMax must be greater than or equal to JMin"));
        }
        success = FALSE;
    }

    return success;
}

// callback when input has changed
static void
stuff_changed_cb(GtkWidget *widget, gpointer user_data)
{
    g_signal_emit_by_name(AMNEZIAWG_EDITOR(user_data), "changed");
}

// callback for show private key toggle
static void
show_private_key_toggled(GtkWidget *widget, gpointer user_data)
{
    AmneziaWGEditor *self = AMNEZIAWG_EDITOR(user_data);
    AmneziaWGEditorPrivate *priv = AMNEZIAWG_EDITOR_GET_PRIVATE(self);
    GtkWidget *entry;

    entry = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_private_key_entry"));
    if (entry) {
        gtk_entry_set_visibility(GTK_ENTRY(entry), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
    }
}

// Helper to get string from widget
static gchar *
get_widget_text(GtkBuilder *builder, const char *widget_name)
{
    GtkWidget *widget = GTK_WIDGET(gtk_builder_get_object(builder, widget_name));
    if (!widget)
        return NULL;

    if (GTK_IS_SPIN_BUTTON(widget)) {
        gint value = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(widget));
        return g_strdup_printf("%d", value);
    }

    const char *str = AWG_EDITABLE_GET_TEXT(GTK_ENTRY(widget));
    return (str && str[0]) ? g_strdup(str) : NULL;
}

// Helper to set widget text
static void
set_widget_text(GtkBuilder *builder, const char *widget_name, const char *text)
{
    GtkWidget *widget = GTK_WIDGET(gtk_builder_get_object(builder, widget_name));
    if (!widget)
        return;

    if (GTK_IS_SPIN_BUTTON(widget)) {
        if (text && text[0]) {
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), g_ascii_strtod(text, NULL));
        } else {
            gtk_spin_button_set_value(GTK_SPIN_BUTTON(widget), 0);
        }
    } else {
        AWG_EDITABLE_SET_TEXT(GTK_EDITABLE(widget), text ? text : "");
    }
}

// Helper: fill interface fields from NMConnection
static void
fill_interface_from_connection(AmneziaWGEditor *self)
{
    AmneziaWGEditorPrivate *priv = AMNEZIAWG_EDITOR_GET_PRIVATE(self);
    NMSettingVpn *s_vpn;
    const char *str;

    s_vpn = NM_SETTING_VPN(nm_connection_get_setting(priv->connection, NM_TYPE_SETTING_VPN));
    if (!s_vpn)
        return;

    str = nm_setting_vpn_get_secret(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY);
    set_widget_text(priv->builder, "interface_private_key_entry", str ?: "");

    {
        GtkWidget *entry = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_private_key_entry"));
        if (entry) {
            gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);
        }
    }

    {
        NMSettingSecretFlags flags;
        if (nm_setting_get_secret_flags(NM_SETTING(s_vpn), NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY, &flags, NULL)) {
            GtkWidget *entry = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_private_key_entry"));
            if (entry) {
                nma_utils_update_password_storage(entry, flags, NM_SETTING(s_vpn), NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY);
            }
        }
    }

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_LISTEN_PORT);
    set_widget_text(priv->builder, "interface_port_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_MTU);
    set_widget_text(priv->builder, "interface_mtu_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_FW_MARK);
    set_widget_text(priv->builder, "interface_fwmark_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_JC);
    set_widget_text(priv->builder, "interface_jc_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_JMIN);
    set_widget_text(priv->builder, "interface_jmin_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_JMAX);
    set_widget_text(priv->builder, "interface_jmax_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_S1);
    set_widget_text(priv->builder, "interface_s1_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_S2);
    set_widget_text(priv->builder, "interface_s2_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_S3);
    set_widget_text(priv->builder, "interface_s3_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_S4);
    set_widget_text(priv->builder, "interface_s4_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H1);
    set_widget_text(priv->builder, "interface_h1_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H2);
    set_widget_text(priv->builder, "interface_h2_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H3);
    set_widget_text(priv->builder, "interface_h3_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H4);
    set_widget_text(priv->builder, "interface_h4_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_I1);
    set_widget_text(priv->builder, "interface_i1_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_I2);
    set_widget_text(priv->builder, "interface_i2_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_I3);
    set_widget_text(priv->builder, "interface_i3_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_I4);
    set_widget_text(priv->builder, "interface_i4_entry", str ?: "");

    str = nm_setting_vpn_get_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_I5);
    set_widget_text(priv->builder, "interface_i5_entry", str ?: "");
}

typedef struct {
    AWGDevicePeer *peer;
    GtkWidget *dialog;
    GtkEntry *entry_public_key;
    GtkEntry *entry_allowed_ips;
    GtkEntry *entry_endpoint;
    GtkEntry *entry_psk;
    GtkSpinButton *spin_keepalive;
    GtkInfoBar *info_bar;
    GtkLabel *label_info;
    GtkButton *button_apply;
    GtkToggleButton *toggle_show_psk;
    GtkToggleButton *toggle_advanced_security;
    gboolean is_new;
} PeerDialogData;

static void
peer_dialog_data_free(PeerDialogData *data)
{
    if (data->peer)
        g_object_unref(data->peer);
    g_free(data);
}

static void
peer_dialog_update_ui(PeerDialogData *data)
{
    AWG_EDITABLE_SET_TEXT(GTK_EDITABLE(data->entry_public_key), awg_device_peer_get_public_key(data->peer) ?: "");
    AWG_EDITABLE_SET_TEXT(GTK_EDITABLE(data->entry_allowed_ips), awg_device_peer_get_allowed_ips_as_string(data->peer) ?: "");
    AWG_EDITABLE_SET_TEXT(GTK_EDITABLE(data->entry_endpoint), awg_device_peer_get_endpoint(data->peer) ?: "");
    AWG_EDITABLE_SET_TEXT(GTK_EDITABLE(data->entry_psk), awg_device_peer_get_shared_key(data->peer) ?: "");

    guint16 ka = awg_device_peer_get_keep_alive_interval(data->peer);
    gtk_spin_button_set_value(data->spin_keepalive, ka);

    gtk_toggle_button_set_active(data->toggle_advanced_security, awg_device_peer_get_advanced_security(data->peer));
}

static void
peer_dialog_update_peer(PeerDialogData *data)
{
    const char *str;

    str = AWG_EDITABLE_GET_TEXT(GTK_EDITABLE(data->entry_public_key));
    awg_device_peer_set_public_key(data->peer, str && str[0] ? str : NULL);

    str = AWG_EDITABLE_GET_TEXT(GTK_EDITABLE(data->entry_allowed_ips));
    awg_device_peer_set_allowed_ips_from_string(data->peer, str);

    str = AWG_EDITABLE_GET_TEXT(GTK_EDITABLE(data->entry_endpoint));
    awg_device_peer_set_endpoint(data->peer, str && str[0] ? str : NULL);

    str = AWG_EDITABLE_GET_TEXT(GTK_EDITABLE(data->entry_psk));
    awg_device_peer_set_shared_key(data->peer, str && str[0] ? str : NULL);

    if (data->entry_psk) {
        NMSettingSecretFlags flags = nma_utils_menu_to_secret_flags(GTK_WIDGET(data->entry_psk));
        awg_device_peer_set_shared_key_flags(data->peer, flags);
    }

    gint keepalive = gtk_spin_button_get_value_as_int(data->spin_keepalive);
    awg_device_peer_set_keep_alive_interval(data->peer, keepalive);

    awg_device_peer_set_advanced_security(data->peer, gtk_toggle_button_get_active(data->toggle_advanced_security));
}

static void
peer_dialog_apply_clicked(GtkWidget *widget, PeerDialogData *data)
{
    gtk_dialog_response(GTK_DIALOG(data->dialog), GTK_RESPONSE_APPLY);
}

static void
peer_dialog_cancel_clicked(GtkWidget *widget, PeerDialogData *data)
{
    gtk_dialog_response(GTK_DIALOG(data->dialog), GTK_RESPONSE_CANCEL);
}

static void
peer_dialog_show_psk_toggled(GtkWidget *widget, PeerDialogData *data)
{
    gtk_entry_set_visibility(data->entry_psk, gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

typedef struct {
    AmneziaWGEditor *editor;
    AWGDevicePeer *peer;
    gboolean is_new;
} PeerDialogInfo;

static void
peer_dialog_response(GtkDialog *dialog, gint response_id, PeerDialogInfo *info);

static GtkWidget *
peer_dialog_create(GtkWidget *toplevel, AWGDevicePeer *peer, gboolean is_new, AmneziaWGEditor *editor)
{
    PeerDialogData *data;
    PeerDialogInfo *info;
    GtkWidget *dialog;
    GtkBuilder *builder;
    GtkWidget *button_cancel;

    builder = gtk_builder_new();

    gtk_builder_set_translation_domain(builder, GETTEXT_PACKAGE);

    if (!gtk_builder_add_from_resource(builder, "/org/freedesktop/network-manager-amneziawg/nm-amneziawg-dialog.ui", NULL)) {
        g_object_unref(builder);
        return NULL;
    }

    dialog = GTK_WIDGET(gtk_builder_get_object(builder, "PeerDialog"));
    if (!dialog) {
        g_object_unref(builder);
        return NULL;
    }

    data = g_new0(PeerDialogData, 1);
    data->peer = g_object_ref(peer);
    data->dialog = dialog;
    data->is_new = is_new;
    data->entry_public_key = GTK_ENTRY(gtk_builder_get_object(builder, "peer_dialog_public_key"));
    data->entry_allowed_ips = GTK_ENTRY(gtk_builder_get_object(builder, "peer_dialog_allowed_ips"));
    data->entry_endpoint = GTK_ENTRY(gtk_builder_get_object(builder, "peer_dialog_endpoint"));
    data->entry_psk = GTK_ENTRY(gtk_builder_get_object(builder, "peer_dialog_psk"));
    data->spin_keepalive = GTK_SPIN_BUTTON(gtk_builder_get_object(builder, "peer_dialog_keepalive"));
    data->info_bar = GTK_INFO_BAR(gtk_builder_get_object(builder, "peer_dialog_info_bar"));
    data->label_info = GTK_LABEL(gtk_builder_get_object(builder, "peer_dialog_label_info"));
    data->button_apply = GTK_BUTTON(gtk_builder_get_object(builder, "peer_dialog_button_apply"));
    data->toggle_show_psk = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "peer_dialog_show_psk"));
    data->toggle_advanced_security = GTK_TOGGLE_BUTTON(gtk_builder_get_object(builder, "peer_dialog_advanced_security"));
    button_cancel = GTK_WIDGET(gtk_builder_get_object(builder, "peer_dialog_button_cancel"));

    g_object_set_data_full(G_OBJECT(dialog), "peer-data", data, (GDestroyNotify)peer_dialog_data_free);

    info = g_new0(PeerDialogInfo, 1);
    info->editor = editor;
    info->peer = peer;
    info->is_new = is_new;
    g_signal_connect_data(G_OBJECT(dialog), "response", G_CALLBACK(peer_dialog_response), info, (GClosureNotify)g_free, 0);

    gtk_spin_button_set_range(data->spin_keepalive, 0, 65535);
    gtk_spin_button_set_value(data->spin_keepalive, 0);

    if (data->entry_psk) {
        gtk_entry_set_visibility(data->entry_psk, FALSE);
        nma_utils_setup_password_storage(GTK_WIDGET(data->entry_psk), 0, NULL, NULL, TRUE, FALSE);
    }

    peer_dialog_update_ui(data);

    g_signal_connect(data->button_apply, "clicked", G_CALLBACK(peer_dialog_apply_clicked), data);
    if (button_cancel)
        g_signal_connect(button_cancel, "clicked", G_CALLBACK(peer_dialog_cancel_clicked), data);
    if (data->toggle_show_psk)
        g_signal_connect(data->toggle_show_psk, "toggled", G_CALLBACK(peer_dialog_show_psk_toggled), data);

    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(toplevel));

    g_object_unref(builder);
    return dialog;
}

static void
peer_dialog_response(GtkDialog *dialog, gint response_id, PeerDialogInfo *info)
{
    if (response_id == GTK_RESPONSE_APPLY) {
        PeerDialogData *data = g_object_get_data(G_OBJECT(dialog), "peer-data");
        if (data) {
            const char *public_key = AWG_EDITABLE_GET_TEXT(GTK_EDITABLE(data->entry_public_key));
            const char *endpoint = AWG_EDITABLE_GET_TEXT(GTK_EDITABLE(data->entry_endpoint));
            const char *psk = AWG_EDITABLE_GET_TEXT(GTK_EDITABLE(data->entry_psk));
            NMSettingSecretFlags psk_flags = awg_device_peer_get_shared_key_flags(data->peer);

            gboolean valid = TRUE;
            GString *error_msg = g_string_new(NULL);

            if (!check_peer_public_key(public_key)) {
                g_string_append(error_msg, _("Public key is required and must be valid base64\n"));
                valid = FALSE;
            }
            if (!check_peer_endpoint(endpoint)) {
                g_string_append(error_msg, _("Endpoint is required and must be valid (host:port)"));
                valid = FALSE;
            }
            const char *allowed_ips = AWG_EDITABLE_GET_TEXT(GTK_EDITABLE(data->entry_allowed_ips));
            if (!check_peer_allowed_ips(allowed_ips)) {
                g_string_append(error_msg, _("Allowed IPs must be valid subnets (e.g., 10.0.0.0/24)"));
                valid = FALSE;
            }

            if (!(psk_flags & (NM_SETTING_SECRET_FLAG_NOT_REQUIRED | NM_SETTING_SECRET_FLAG_NOT_SAVED))) {
                if (!awg_validate_base64(psk)) {
                    g_string_append(error_msg, _("Preshared key must be valid base64 or empty"));
                    valid = FALSE;
                }
            }

            if (!valid) {
                gtk_info_bar_set_message_type(data->info_bar, GTK_MESSAGE_ERROR);
                gtk_label_set_text(data->label_info, error_msg->str);
                gtk_widget_show(GTK_WIDGET(data->info_bar));
                g_string_free(error_msg, TRUE);
                return;
            }

            gtk_widget_hide(GTK_WIDGET(data->info_bar));
            peer_dialog_update_peer(data);
        }
        g_signal_emit_by_name(info->editor, "changed");
    } else if (response_id == GTK_RESPONSE_CANCEL || response_id == GTK_RESPONSE_DELETE_EVENT) {
        if (info->is_new) {
            AmneziaWGEditorPrivate *priv = AMNEZIAWG_EDITOR_GET_PRIVATE(info->editor);
            guint count = awg_device_get_peers_count(priv->device);
            if (count > 0) {
                awg_device_remove_peer(priv->device, count - 1);
            }
        }
    }

#if GTK_CHECK_VERSION(4, 0, 0)
    gtk_window_destroy(GTK_WINDOW(dialog));
#else
    gtk_widget_destroy(GTK_WIDGET(dialog));
#endif
}

// Callback: add peer button clicked - opens dialog
static void
peer_add_button_clicked(GtkButton *button, gpointer user_data)
{
    AmneziaWGEditor *self = AMNEZIAWG_EDITOR(user_data);
    AmneziaWGEditorPrivate *priv = AMNEZIAWG_EDITOR_GET_PRIVATE(self);
    AWGDevicePeer *peer;
    GtkWidget *dialog;
#if GTK_CHECK_VERSION(4, 0, 0)
    GtkRoot *toplevel = gtk_widget_get_root(priv->widget);
#else
    GtkWidget *toplevel = gtk_widget_get_toplevel(priv->widget);
#endif

    peer = awg_device_peer_new();
    awg_device_add_peer(priv->device, peer);

    priv->selected_peer_index = awg_device_get_peers_count(priv->device) - 1;

    GtkWidget *treeview = GTK_WIDGET(gtk_builder_get_object(priv->builder, "peers_treeview"));
    if (treeview) {
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
        if (GTK_IS_LIST_STORE(model)) {
            GtkTreeIter iter;
            gtk_list_store_append(GTK_LIST_STORE(model), &iter);
            gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, "(new peer)", -1);
        }
    }

    dialog = peer_dialog_create(GTK_WIDGET(toplevel), peer, TRUE, self);
    if (!dialog) {
        g_object_unref(peer);
        return;
    }

    GtkWidget *remove_btn = GTK_WIDGET(gtk_builder_get_object(priv->builder, "peer_remove_button"));
    if (remove_btn)
        gtk_widget_set_sensitive(remove_btn, TRUE);

    gtk_widget_show(dialog);
}

// Callback: TreeView selection changed
static void
peers_treeview_selection_changed(GtkTreeSelection *selection, gpointer user_data)
{
    AmneziaWGEditor *self = AMNEZIAWG_EDITOR(user_data);
    AmneziaWGEditorPrivate *priv = AMNEZIAWG_EDITOR_GET_PRIVATE(self);
    GtkWidget *remove_btn;

    remove_btn = GTK_WIDGET(gtk_builder_get_object(priv->builder, "peer_remove_button"));
    if (remove_btn) {
        gtk_widget_set_sensitive(remove_btn, gtk_tree_selection_get_selected(selection, NULL, NULL));
    }
}

// Callback: TreeView row double-clicked to edit peer
static void
peers_treeview_row_activated(GtkTreeView *tree_view, GtkTreePath *path, GtkTreeViewColumn *column, gpointer user_data)
{
    AmneziaWGEditor *self = AMNEZIAWG_EDITOR(user_data);
    AmneziaWGEditorPrivate *priv = AMNEZIAWG_EDITOR_GET_PRIVATE(self);
    GtkWidget *dialog;
    AWGDevicePeer *peer;
    const GList *peers;
    gint *indices;
    guint idx;

    indices = gtk_tree_path_get_indices(path);
    if (!indices)
        return;

    idx = indices[0];
    peers = awg_device_get_peers_list(priv->device);
    peer = AWG_DEVICE_PEER(g_list_nth_data((GList *)peers, idx));
    if (!peer)
        return;

    priv->selected_peer_index = idx;

#if GTK_CHECK_VERSION(4, 0, 0)
    GtkRoot *toplevel = gtk_widget_get_root(priv->widget);
#else
    GtkWidget *toplevel = gtk_widget_get_toplevel(priv->widget);
#endif

    dialog = peer_dialog_create(GTK_WIDGET(toplevel), peer, FALSE, self);
    if (!dialog)
        return;

    gtk_widget_show(dialog);
}

// Callback: remove peer button clicked
static void
peer_remove_button_clicked(GtkButton *button, gpointer user_data)
{
    AmneziaWGEditor *self = AMNEZIAWG_EDITOR(user_data);
    AmneziaWGEditorPrivate *priv = AMNEZIAWG_EDITOR_GET_PRIVATE(self);
    guint count;

    count = awg_device_get_peers_count(priv->device);
    if (count == 0)
        return;

    awg_device_remove_peer(priv->device, priv->selected_peer_index);

    if (priv->selected_peer_index > 0) {
        priv->selected_peer_index--;
    }

    count = awg_device_get_peers_count(priv->device);
    if (count == 0) {
        AWGDevicePeer *peer = awg_device_peer_new();
        awg_device_add_peer(priv->device, peer);
        g_object_unref(peer);
    }

    GtkWidget *treeview = GTK_WIDGET(gtk_builder_get_object(priv->builder, "peers_treeview"));
    if (treeview) {
        GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));
        if (GTK_IS_LIST_STORE(model)) {
            gtk_list_store_clear(GTK_LIST_STORE(model));
            const GList *peers = awg_device_get_peers_list(priv->device);
            for (const GList *l = peers; l; l = l->next) {
                AWGDevicePeer *p = AWG_DEVICE_PEER(l->data);
                const gchar *public_key = awg_device_peer_get_public_key(p);
                const gchar *endpoint = awg_device_peer_get_endpoint(p);
                GtkTreeIter iter;
                gtk_list_store_append(GTK_LIST_STORE(model), &iter);
                if (public_key && public_key[0]) {
                    gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, public_key, -1);
                } else if (endpoint && endpoint[0]) {
                    gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, endpoint, -1);
                } else {
                    gtk_list_store_set(GTK_LIST_STORE(model), &iter, 0, "(new peer)", -1);
                }
            }
        }
    }

    GtkWidget *remove_btn = GTK_WIDGET(gtk_builder_get_object(priv->builder, "peer_remove_button"));
    if (remove_btn)
        gtk_widget_set_sensitive(remove_btn, count > 0);

    GtkWidget *warning_label = GTK_WIDGET(gtk_builder_get_object(priv->builder, "peers_warning_label"));
    if (warning_label)
        gtk_widget_set_visible(warning_label, count == 0);

    g_signal_emit_by_name(self, "changed");
}

static gboolean
init_editor_plugin(AmneziaWGEditor *self, NMConnection *connection, GError **error)
{
    AmneziaWGEditorPrivate *priv = AMNEZIAWG_EDITOR_GET_PRIVATE(self);
    GtkWidget *widget;

    priv->connection = g_object_ref(connection);

    priv->selected_peer_index = 0;

    fill_interface_from_connection(self);

    priv->device = awg_device_new_from_nm_connection(connection, error);
    if (!priv->device) {
        return FALSE;
    }

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_private_key_entry"));
    if (widget) {
        NMSettingVpn *s_vpn = NM_SETTING_VPN(nm_connection_get_setting(connection, NM_TYPE_SETTING_VPN));
        nma_utils_setup_password_storage(widget, 0, NM_SETTING(s_vpn), NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY, FALSE, FALSE);
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);
    }

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_show_private_key"));
    if (widget) {
        g_signal_connect(G_OBJECT(widget), "toggled", G_CALLBACK(show_private_key_toggled), self);
    }

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_mtu_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_fwmark_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_port_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_jc_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_jmin_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_jmax_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_s1_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_s2_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_s3_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_s4_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_h1_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_h2_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_h3_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_h4_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_i1_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_i2_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_i3_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_i4_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_i5_entry"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "changed", G_CALLBACK(stuff_changed_cb), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "peer_add_button"));
    if (widget)
        g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(peer_add_button_clicked), self);

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "peer_remove_button"));
    if (widget) {
        g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(peer_remove_button_clicked), self);
        gtk_widget_set_sensitive(widget, awg_device_get_peers_count(priv->device) > 0);
    }

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "peers_treeview"));
    if (widget) {
        GtkTreeView *treeview = GTK_TREE_VIEW(widget);
        GtkListStore *store = gtk_list_store_new(1, G_TYPE_STRING);
        gtk_tree_view_set_model(treeview, GTK_TREE_MODEL(store));

        GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
        GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(_("Peer"), renderer, "text", 0, NULL);
        gtk_tree_view_append_column(treeview, column);

        const GList *peers = awg_device_get_peers_list(priv->device);
        for (const GList *l = peers; l; l = l->next) {
            AWGDevicePeer *peer = AWG_DEVICE_PEER(l->data);
            const gchar *public_key = awg_device_peer_get_public_key(peer);
            const gchar *endpoint = awg_device_peer_get_endpoint(peer);
            GtkTreeIter iter;
            gtk_list_store_append(store, &iter);
            if (endpoint && endpoint[0] && public_key && public_key[0]) {
                gchar *label = g_strdup_printf("%s - %.8s...", endpoint, public_key);
                gtk_list_store_set(store, &iter, 0, label, -1);
                g_free(label);
            } else if (endpoint && endpoint[0]) {
                gtk_list_store_set(store, &iter, 0, endpoint, -1);
            } else if (public_key && public_key[0]) {
                gtk_list_store_set(store, &iter, 0, public_key, -1);
            } else {
                gtk_list_store_set(store, &iter, 0, "(new peer)", -1);
            }
        }

        g_signal_connect(G_OBJECT(widget), "row-activated", G_CALLBACK(peers_treeview_row_activated), self);

        GtkTreeSelection *selection = gtk_tree_view_get_selection(treeview);
        g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(peers_treeview_selection_changed), self);
    }

    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "peer_remove_button"));
    if (widget) {
        gtk_widget_set_sensitive(widget, FALSE);
    }

    if (awg_device_get_peers_count(priv->device) == 0) {
        AWGDevicePeer *peer = awg_device_peer_new();
        awg_device_add_peer(priv->device, peer);
        g_object_unref(peer);
    }

    return TRUE;
}

// get the active widget (config GUI)
static GObject *
get_widget(NMVpnEditor *iface)
{
    AmneziaWGEditor *self = AMNEZIAWG_EDITOR(iface);
    AmneziaWGEditorPrivate *priv = AMNEZIAWG_EDITOR_GET_PRIVATE(self);

    return G_OBJECT(priv->widget);
}

// OLD update_connection removed - see new implementation below
// check if the user's inputs are valid and if so, update the NMConnection's
// NMSettingVpn data items (gets called everytime something changes, afaik)
static gboolean
update_connection(NMVpnEditor *iface,
                  NMConnection *connection,
                  GError **error);

// Save widget values to NMSettingVpn
static void
save_interface_to_connection(AmneziaWGEditor *self)
{
    AmneziaWGEditorPrivate *priv = AMNEZIAWG_EDITOR_GET_PRIVATE(self);
    NMSettingVpn *s_vpn;
    gchar *str;
    GtkWidget *widget;

    s_vpn = NM_SETTING_VPN(nm_connection_get_setting(priv->connection, NM_TYPE_SETTING_VPN));
    if (!s_vpn) {
        s_vpn = NM_SETTING_VPN(nm_setting_vpn_new());
        nm_connection_add_setting(priv->connection, NM_SETTING(s_vpn));
        g_object_set(s_vpn, NM_SETTING_VPN_SERVICE_TYPE, NM_AWG_VPN_SERVICE_TYPE, NULL);
    }

    str = get_widget_text(priv->builder, "interface_private_key_entry");
    widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "interface_private_key_entry"));
    if (widget) {
        NMSettingSecretFlags flags = nma_utils_menu_to_secret_flags(widget);
        nm_setting_set_secret_flags(NM_SETTING(s_vpn), NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY, flags, NULL);
        nma_utils_update_password_storage(widget, flags, NM_SETTING(s_vpn), NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY);

        if (flags & (NM_SETTING_SECRET_FLAG_NOT_REQUIRED | NM_SETTING_SECRET_FLAG_NOT_SAVED)) {
            nm_setting_vpn_remove_secret(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY);
        } else if (str && str[0]) {
            nm_setting_vpn_add_secret(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_PRIVATE_KEY, str);
        }
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_port_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_LISTEN_PORT, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_mtu_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_MTU, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_fwmark_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_FW_MARK, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_jc_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_JC, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_jmin_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_JMIN, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_jmax_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_JMAX, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_s1_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_S1, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_s2_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_S2, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_s3_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_S3, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_s4_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_S4, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_h1_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H1, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_h2_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H2, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_h3_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H3, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_h4_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_H4, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_i1_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_I1, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_i2_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_I2, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_i3_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_I3, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_i4_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_I4, str);
    }
    g_free(str);

    str = get_widget_text(priv->builder, "interface_i5_entry");
    if (str && str[0]) {
        nm_setting_vpn_add_data_item(s_vpn, NM_AWG_VPN_CONFIG_DEVICE_I5, str);
    }
    g_free(str);

    if (priv->device) {
        const GList *peers = awg_device_get_peers_list(priv->device);
        guint i = 0;
        for (const GList *l = peers; l; l = l->next, i++) {
            AWGDevicePeer *peer = AWG_DEVICE_PEER(l->data);
            gchar *key;
            const gchar *value;

            key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PUBLIC_KEY, i);
            value = awg_device_peer_get_public_key(peer);
            if (value)
                nm_setting_vpn_add_data_item(s_vpn, key, value);
            g_free(key);

            key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PRESHARED_KEY, i);
            value = awg_device_peer_get_shared_key(peer);
            if (value)
                nm_setting_vpn_add_secret(s_vpn, key, value);
            g_free(key);

            key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_PRESHARED_KEY_FLAGS, i);
            nm_setting_set_secret_flags(NM_SETTING(s_vpn), key,
                                        awg_device_peer_get_shared_key_flags(peer), NULL);
            g_free(key);

            key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_ENDPOINT, i);
            value = awg_device_peer_get_endpoint(peer);
            if (value)
                nm_setting_vpn_add_data_item(s_vpn, key, value);
            g_free(key);

            key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_ALLOWED_IPS, i);
            value = awg_device_peer_get_allowed_ips_as_string(peer);
            if (value)
                nm_setting_vpn_add_data_item(s_vpn, key, value);
            g_free(key);

            key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_KEEP_ALIVE, i);
            guint16 keep_alive = awg_device_peer_get_keep_alive_interval(peer);
            nm_setting_vpn_add_data_item(s_vpn, key, g_strdup_printf("%u", keep_alive));
            g_free(key);

            key = g_strdup_printf(NM_AWG_VPN_CONFIG_PEER_ADVANCED_SECURITY, i);
            nm_setting_vpn_add_data_item(s_vpn, key, awg_device_peer_get_advanced_security(peer) ? "on" : "off");
            g_free(key);
        }
    }
}

// check if the user's inputs are valid and if so, update the NMConnection's
// NMSettingVpn data items (gets called everytime something changes, afaik)
static gboolean
update_connection(NMVpnEditor *iface,
                  NMConnection *connection,
                  GError **error)
{
    AmneziaWGEditor *self = AMNEZIAWG_EDITOR(iface);

    if (!check_validity(self, error)) {
        return FALSE;
    }

    save_interface_to_connection(self);

    return TRUE;
}

// function to determine if the connection is new, according to its data items
static void
is_new_func(const char *key, const char *value, gpointer user_data)
{
    gboolean *is_new = user_data;

    /* If there are any VPN data items the connection isn't new */
    *is_new = FALSE;
}

/*****************************************************************************/

static void
amneziawg_editor_plugin_widget_init(AmneziaWGEditor *plugin)
{
}

NMVpnEditor *
amneziawg_editor_new(NMConnection *connection, GError **error)
{
    NMVpnEditor *object;
    AmneziaWGEditorPrivate *priv;
    gboolean new = TRUE;
    NMSettingVpn *s_vpn;

    if (error)
        g_return_val_if_fail(*error == NULL, NULL);

    object = g_object_new(AMNEZIAWG_TYPE_EDITOR, NULL);
    if (!object) {
        g_set_error_literal(error, NMV_EDITOR_PLUGIN_ERROR, 0, _("Could not create amneziawg object"));
        return NULL;
    }

    priv = AMNEZIAWG_EDITOR_GET_PRIVATE(object);

    priv->builder = gtk_builder_new();

    gtk_builder_set_translation_domain(priv->builder, GETTEXT_PACKAGE);

    // create the GUI from our .ui file
    // note: the resource is described in gresource.xml and gets compiled to resources.c
    if (!gtk_builder_add_from_resource(priv->builder, "/org/freedesktop/network-manager-amneziawg/nm-amneziawg-dialog.ui", error)) {
        g_object_unref(object);
        g_return_val_if_reached(NULL);
    }

    priv->widget = GTK_WIDGET(gtk_builder_get_object(priv->builder, "wg-vbox"));
    if (!priv->widget) {
        g_set_error_literal(error, NMV_EDITOR_PLUGIN_ERROR, 0, _("could not load UI widget"));
        g_object_unref(object);
        g_return_val_if_reached(NULL);
    }
    g_object_ref_sink(priv->widget);

    s_vpn = nm_connection_get_setting_vpn(connection);
    // if there is at least one item to iterate over, the connection can't be new
    if (s_vpn)
        nm_setting_vpn_foreach_data_item(s_vpn, is_new_func, &new);
    priv->new_connection = new;

    if (!init_editor_plugin(AMNEZIAWG_EDITOR(object), connection, error)) {
        g_object_unref(object);
        return NULL;
    }

    return object;
}

static void
dispose(GObject *object)
{
    AmneziaWGEditor *plugin = AMNEZIAWG_EDITOR(object);
    AmneziaWGEditorPrivate *priv = AMNEZIAWG_EDITOR_GET_PRIVATE(plugin);

    g_clear_object(&priv->widget);

    g_clear_object(&priv->builder);

    g_clear_object(&priv->connection);

    g_clear_object(&priv->device);

    G_OBJECT_CLASS(amneziawg_editor_plugin_widget_parent_class)->dispose(object);
}

static void
amneziawg_editor_plugin_widget_interface_init(NMVpnEditorInterface *iface_class)
{
    /* interface implementation */
    iface_class->get_widget = get_widget;
    iface_class->update_connection = update_connection;
}

static void
amneziawg_editor_plugin_widget_class_init(AmneziaWGEditorClass *req_class)
{
    GObjectClass *object_class = G_OBJECT_CLASS(req_class);

    g_type_class_add_private(req_class, sizeof(AmneziaWGEditorPrivate));

    object_class->dispose = dispose;
}

/*****************************************************************************/

NMVpnEditor *
nm_vpn_editor_factory_amneziawg(NMVpnEditorPlugin *editor_plugin,
                                NMConnection *connection,
                                GError **error)
{
    g_return_val_if_fail(!error || !*error, NULL);

    return amneziawg_editor_new(connection, error);
}
