/*
 * Copyright 2016-2019, Intel Corporation
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
 * rpmem_cmd.h -- helper module for invoking separate process
 */

#ifndef RPMEM_CMD_H
#define RPMEM_CMD_H 1

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rpmem_cmd {
	int fd_in;	/* stdin */
	int fd_out;	/* stdout */
	int fd_err;	/* stderr */
	struct {
		char **argv;
		int argc;
	} args;		/* command arguments */
	pid_t pid;	/* pid of process */
};

struct rpmem_cmd *rpmem_cmd_init(void);
int rpmem_cmd_push(struct rpmem_cmd *cmd, const char *arg);
int rpmem_cmd_run(struct rpmem_cmd *cmd);
void rpmem_cmd_term(struct rpmem_cmd *cmd);
int rpmem_cmd_wait(struct rpmem_cmd *cmd, int *status);
void rpmem_cmd_fini(struct rpmem_cmd *cmd);

#ifdef __cplusplus
}
#endif

#endif
