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

#ifndef AWG_VALIDATE_H
#define AWG_VALIDATE_H

#include <glib.h>
#include <stdint.h>

#define AWG_JC_MIN 0
#define AWG_JC_MAX UINT16_MAX
#define AWG_JMIN_MIN 0
#define AWG_JMIN_MAX UINT16_MAX
#define AWG_JMAX_MIN 0
#define AWG_JMAX_MAX 65534
#define AWG_JUNK_SIZE_MIN 0
#define AWG_S3_MAX 65479
#define AWG_S4_MAX 65503
#define AWG_JUNK_SIZE_MAX UINT16_MAX
#define MAX_AWG_STRING_LEN (5 * 1024)

gboolean awg_validate_empty(const gchar *str);

gboolean awg_validate_awg_string(const gchar *str);

gboolean awg_validate_ip4(const gchar *addr);

gboolean awg_validate_ip6(const gchar *addr);

gboolean awg_validate_port(const gchar *str);

gboolean awg_validate_mtu(const gchar *str);

gboolean awg_validate_jc(const gchar *str);

gboolean awg_validate_jmin(const gchar *str);

gboolean awg_validate_jmax(const gchar *str);

gboolean awg_validate_junk_size(const gchar *str);

gboolean awg_validate_base64(const gchar *str);

gboolean awg_validate_fqdn(const gchar *str);

gboolean awg_validate_subnet(const gchar *str);

gboolean awg_validate_allowed_ips(const gchar *str);

gboolean awg_validate_endpoint(const gchar *str);

gboolean awg_validate_fw_mark(const gchar *str);

gboolean awg_validate_magic_header(const gchar *str);

gboolean awg_magic_header_parse(const gchar *str, guint32 *start, guint32 *end);

gboolean awg_validate_magic_headers_no_overlap(const gchar *h1, const gchar *h2, const gchar *h3, const gchar *h4);

gboolean awg_validate_i_packet(const gchar *str);

gboolean awg_validate_s3(const gchar *str);

gboolean awg_validate_s4(const gchar *str);

gboolean awg_validate_jmin_jmax(guint16 jmin, guint16 jmax);

#endif /* AWG_VALIDATE_H */
