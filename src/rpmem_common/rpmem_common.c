/*
 * Copyright 2016, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * rpmem_common.c -- common definitions for librpmem and rpmemd
 */

#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/tcp.h>

#include "rpmem_common.h"
#include "rpmem_proto.h"
#include "rpmem_common_log.h"
#include "base64.h"

/*
 * rpmem_xwrite -- send entire buffer or fail
 *
 * Returns 1 if send returned 0.
 */
int
rpmem_xwrite(int fd, const void *buf, size_t len, int flags)
{
	size_t wr = 0;
	const uint8_t *cbuf = buf;
	while (wr < len) {
		ssize_t sret;
		if (!flags)
			sret = write(fd, &cbuf[wr], len - wr);
		else
			sret = send(fd, &cbuf[wr], len - wr, flags);

		if (sret == 0)
			return 1;

		if (sret < 0)
			return (int)sret;

		wr += (size_t)sret;
	}

	return 0;
}

/*
 * rpmem_xread -- read entire buffer or fail
 *
 * Returns 1 if recv returned 0.
 */
int
rpmem_xread(int fd, void *buf, size_t len, int flags)
{
	size_t rd = 0;
	uint8_t *cbuf = buf;
	while (rd < len) {
		ssize_t sret;

		if (!flags)
			sret = read(fd, &cbuf[rd], len - rd);
		else
			sret = recv(fd, &cbuf[rd], len - rd, flags);

		if (sret == 0)
			return 1;

		if (sret < 0)
			return (int)sret;

		rd += (size_t)sret;
	}

	return 0;
}

/*
 * rpmemd_b64_read -- read entire buffer or fail, decode using base64
 */
int
rpmem_b64_read(int fd, void *buff, size_t len, int flags)
{
	int ret;

	size_t b64_len;
	void *b64_buff = base64_buff(len, &b64_len);
	if (!b64_buff)
		return -1;

	ret = rpmem_xread(fd, b64_buff, b64_len, flags);
	if (!ret)
		ret = base64_decode(b64_buff, b64_len, buff, len);

	free(b64_buff);

	return ret;
}

/*
 * rpmemd_b64_write -- write entire buffer or fail, encode using base64
 */
int
rpmem_b64_write(int fd, const void *buff, size_t len, int flags)
{
	int ret;

	size_t b64_len;
	void *b64_buff = base64_buff(len, &b64_len);
	if (!b64_buff)
		return -1;
	ret = base64_encode(buff, len, b64_buff, b64_len);
	if (ret)
		return ret;

	ret = rpmem_xwrite(fd, b64_buff, b64_len, flags);

	free(b64_buff);

	return ret;
}


static const char *provider2str[MAX_RPMEM_PROV] = {
	[RPMEM_PROV_LIBFABRIC_VERBS] = "verbs",
	[RPMEM_PROV_LIBFABRIC_SOCKETS] = "sockets",
};

/*
 * rpmem_provider_from_str -- convert string to enum rpmem_provider
 *
 * Returns RPMEM_PROV_UNKNOWN if provider is not known.
 */
enum rpmem_provider
rpmem_provider_from_str(const char *str)
{
	for (enum rpmem_provider p = 0; p < MAX_RPMEM_PROV; p++) {
		if (provider2str[p] && strcmp(str, provider2str[p]) == 0)
			return p;
	}

	return RPMEM_PROV_UNKNOWN;
}

/*
 * rpmem_provider_to_str -- convert enum rpmem_provider to string
 */
const char *
rpmem_provider_to_str(enum rpmem_provider provider)
{
	if (provider >= MAX_RPMEM_PROV)
		return NULL;

	return provider2str[provider];
}

/*
 * rpmem_get_ip_str -- converts socket address to string
 */
const char *
rpmem_get_ip_str(const struct sockaddr *addr)
{
	static char str[INET6_ADDRSTRLEN + NI_MAXSERV + 1];
	char ip[INET6_ADDRSTRLEN];
	struct sockaddr_in *in4;

	switch (addr->sa_family) {
	case AF_INET:
		in4 = (struct sockaddr_in *)addr;
		if (!inet_ntop(AF_INET, &in4->sin_addr, ip, sizeof(ip)))
			return NULL;
		if (snprintf(str, sizeof(str), "%s:%u",
				ip, ntohs(in4->sin_port)) < 0)
			return NULL;
		break;
	case AF_INET6:
		/* IPv6 not supported */
	default:
		return NULL;
	}

	return str;
}

/*
 * rpmem_target_split -- split target into user, node and service
 *
 * The user, node and service must be freed by the caller.
 */
int
rpmem_target_split(const char *target, char **user,
	char **node, char **service)
{
	if (user)
		*user = NULL;
	if (node)
		*node = NULL;

	if (service)
		*service = NULL;

	char *target_dup = strdup(target);
	if (!target_dup)
		goto err_target_dup;

	char *u = NULL;
	char *n = strchr(target_dup, '@');
	if (n) {
		u = target_dup;
		*n = '\0';
		n++;
	} else {
		n = target_dup;
	}

	char *s = strchr(n, ':');
	if (s) {
		*s = '\0';
		s++;
	}

	if (node) {
		*node = strdup(n);
		if (!(*node))
			goto err_dup_node;
	}

	if (u && user) {
		*user = strdup(u);
		if (!(*user))
			goto err_dup_user;
	}

	if (s && service) {
		*service = strdup(s);
		if (!(*service))
			goto err_dup_service;
	}

	free(target_dup);

	return 0;
err_dup_service:
	if (user)
		free(*user);
err_dup_user:
	if (node)
		free(*node);
err_dup_node:
	free(target_dup);
err_target_dup:
	return -1;
}
