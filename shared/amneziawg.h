/* SPDX-License-Identifier: LGPL-2.1+
 *
 * Copyright (C) 2015-2018 Jason A. Donenfeld <Jason@zx2c4.com>. All Rights Reserved.
 */

#ifndef AMNEZIAWG_H
#define AMNEZIAWG_H

#include <net/if.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

typedef uint8_t wg_key[32];
typedef char wg_key_b64_string[((sizeof(wg_key) + 2) / 3) * 4 + 1];

typedef struct awg_allowedip {
	uint16_t family;
	union {
		struct in_addr ip4;
		struct in6_addr ip6;
	};
	uint8_t cidr;
	struct awg_allowedip *next_allowedip;
} wg_allowedip;

enum wg_peer_flags {
	AWGPEER_REMOVE_ME = 1U << 0,
	AWGPEER_REPLACE_ALLOWEDIPS = 1U << 1,
	AWGPEER_HAS_PUBLIC_KEY = 1U << 2,
	AWGPEER_HAS_PRESHARED_KEY = 1U << 3,
	AWGPEER_HAS_PERSISTENT_KEEPALIVE_INTERVAL = 1U << 4
};

typedef struct awg_peer {
	enum wg_peer_flags flags;

	wg_key public_key;
	wg_key preshared_key;

	union {
		struct sockaddr addr;
		struct sockaddr_in addr4;
		struct sockaddr_in6 addr6;
	} endpoint;

	struct timespec last_handshake_time;
	uint64_t rx_bytes, tx_bytes;
	uint16_t persistent_keepalive_interval;

	struct awg_allowedip *first_allowedip, *last_allowedip;
	struct awg_peer *next_peer;
} awg_peer;

enum awg_device_flags {
	AWGDEVICE_REPLACE_PEERS = 1U << 0,
	AWGDEVICE_HAS_PRIVATE_KEY = 1U << 1,
	AWGDEVICE_HAS_PUBLIC_KEY = 1U << 2,
	AWGDEVICE_HAS_LISTEN_PORT = 1U << 3,
	AWGDEVICE_HAS_FWMARK = 1U << 4
};

typedef struct awg_device {
	char name[IFNAMSIZ];
	uint32_t ifindex;

	enum awg_device_flags flags;

	wg_key public_key;
	wg_key private_key;

	uint32_t fwmark;
	uint16_t listen_port;

	struct awg_peer *first_peer, *last_peer;
} awg_device;

#define wg_for_each_device_name(__names, __name, __len) for ((__name) = (__names), (__len) = 0; ((__len) = strlen(__name)); (__name) += (__len) + 1)
#define wg_for_each_peer(__dev, __peer) for ((__peer) = (__dev)->first_peer; (__peer); (__peer) = (__peer)->next_peer)
#define wg_for_each_allowedip(__peer, __allowedip) for ((__allowedip) = (__peer)->first_allowedip; (__allowedip); (__allowedip) = (__allowedip)->next_allowedip)

int awg_set_device(awg_device *dev);
int awg_get_device(awg_device **dev, const char *device_name);
int awg_add_device(const char *device_name);
int awg_del_device(const char *device_name);
void awg_free_device(awg_device *dev);
char *awg_list_device_names(void); /* first\0second\0third\0forth\0last\0\0 */
void awg_key_to_base64(wg_key_b64_string base64, const wg_key key);
int awg_key_from_base64(wg_key key, const wg_key_b64_string base64);
bool awg_key_is_zero(const wg_key key);
void awg_generate_public_key(wg_key public_key, const wg_key private_key);
void awg_generate_private_key(wg_key private_key);
void awg_generate_preshared_key(wg_key preshared_key);

#endif
