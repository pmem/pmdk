/*
 * Copyright 2016-2017, Intel Corporation
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
 * rpmem_obc_test_common.c -- common definitions for rpmem_obc tests
 */

#include <sys/socket.h>
#include <netinet/in.h>

#include "rpmem_obc_test_common.h"
#include "os.h"

#define CMD_BUFF_SIZE	4096
static const char *rpmem_cmd;

/*
 * set_rpmem_cmd -- set RPMEM_CMD variable
 */
void
set_rpmem_cmd(const char *fmt, ...)
{
	static char cmd_buff[CMD_BUFF_SIZE];

	if (!rpmem_cmd) {
		char *cmd = os_getenv(RPMEM_CMD_ENV);
		UT_ASSERTne(cmd, NULL);
		rpmem_cmd = STRDUP(cmd);
	}

	ssize_t ret;
	size_t cnt = 0;

	va_list ap;
	va_start(ap, fmt);
	ret = snprintf(&cmd_buff[cnt], CMD_BUFF_SIZE - cnt,
			"%s ", rpmem_cmd);
	UT_ASSERT(ret > 0);
	cnt += (size_t)ret;

	ret = vsnprintf(&cmd_buff[cnt], CMD_BUFF_SIZE - cnt, fmt, ap);
	UT_ASSERT(ret > 0);
	cnt += (size_t)ret;

	va_end(ap);

	ret = os_setenv(RPMEM_CMD_ENV, cmd_buff, 1);
	UT_ASSERTeq(ret, 0);

	/*
	 * Rpmem has internal RPMEM_CMD variable copy and it is assumed
	 * RPMEMD_CMD will not change its value during execution. To refresh the
	 * internal copy it must be destroyed and a instance must be initialized
	 * manually.
	 */
	rpmem_util_cmds_fini();
	rpmem_util_cmds_init();
}

struct server *
srv_init(void)
{
	struct server *s = MALLOC(sizeof(*s));

	s->fd_in = STDIN_FILENO;
	s->fd_out = STDOUT_FILENO;

	uint32_t status = 0;
	srv_send(s, &status, sizeof(status));

	return s;
}

/*
 * srv_stop -- close the server
 */
void
srv_fini(struct server *s)
{
	FREE(s);
}

/*
 * srv_recv -- read a message from the client
 */
void
srv_recv(struct server *s, void *buff, size_t len)
{
	size_t rd = 0;

	uint8_t *cbuf = buff;
	while (rd < len) {
		ssize_t ret = read(s->fd_in, &cbuf[rd], len - rd);
		UT_ASSERT(ret > 0);
		rd += (size_t)ret;
	}
}

/*
 * srv_send -- send a message to the client
 */
void
srv_send(struct server *s, const void *buff, size_t len)
{
	size_t wr = 0;

	const uint8_t *cbuf = buff;
	while (wr < len) {
		ssize_t ret = write(s->fd_out, &cbuf[wr], len - wr);
		UT_ASSERT(ret > 0);
		wr += (size_t)ret;
	}
}

/*
 * client_connect_wait -- wait until client connects to the server
 */
void
client_connect_wait(struct rpmem_obc *rpc, char *target)
{
	struct rpmem_target_info *info;
	info = rpmem_target_parse(target);
	UT_ASSERTne(info, NULL);

	while (rpmem_obc_connect(rpc, info))
		;

	rpmem_target_free(info);
}

/*
 * server_econnreset -- disconnect from client during performing an
 * operation
 */
void
server_econnreset(struct server *s, const void *msg, size_t len)
{
	for (int i = 0; i < ECONNRESET_LOOP; i++) {
		srv_send(s, msg, len);
	}
}
