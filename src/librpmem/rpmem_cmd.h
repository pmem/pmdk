/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2016-2020, Intel Corporation */

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
