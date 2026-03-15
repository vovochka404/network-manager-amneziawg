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
#define AWG_JUNK_SIZE_MIN 0
#define AWG_JUNK_SIZE_MAX UINT16_MAX
#define AWG_HEADER_SIZE_MIN 0
#define AWG_HEADER_SIZE_MAX UINT32_MAX

gboolean awg_validate_empty(const gchar *str);

gboolean awg_validate_ip4(const gchar *addr);

gboolean awg_validate_ip6(const gchar *addr);

gboolean awg_validate_ip_or_empty(const gchar *addr);

gboolean awg_validate_dns(const gchar *str);

gboolean awg_validate_port(const gchar *str);

gboolean awg_validate_mtu(const gchar *str);

gboolean awg_validate_jc(const gchar *str);

gboolean awg_validate_junk_size(const gchar *str);

gboolean awg_validate_header_size(const gchar *str);

gboolean awg_validate_keep_alive(const gchar *str);

gboolean awg_validate_base64(const gchar *str);

gboolean awg_validate_fqdn(const gchar *str);

gboolean awg_validate_subnet(const gchar *str);

gboolean awg_validate_allowed_ips(const gchar *str);

gboolean awg_validate_endpoint(const gchar *str);

gboolean awg_validate_fw_mark(const gchar *str);

#endif /* AWG_VALIDATE_H */
