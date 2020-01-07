// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2016-2018, Intel Corporation */

/*
 * rpmem_ssh.h -- rpmem ssh transport layer header file
 */
#ifndef RPMEM_SSH_H
#define RPMEM_SSH_H 1

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rpmem_ssh;

struct rpmem_ssh *rpmem_ssh_open(const struct rpmem_target_info *info);
struct rpmem_ssh *rpmem_ssh_exec(const struct rpmem_target_info *info, ...);
struct rpmem_ssh *rpmem_ssh_execv(const struct rpmem_target_info *info,
		const char **argv);
int rpmem_ssh_close(struct rpmem_ssh *rps);

int rpmem_ssh_send(struct rpmem_ssh *rps, const void *buff, size_t len);
int rpmem_ssh_recv(struct rpmem_ssh *rps, void *buff, size_t len);
int rpmem_ssh_monitor(struct rpmem_ssh *rps, int nonblock);

const char *rpmem_ssh_strerror(struct rpmem_ssh *rps, int oerrno);

#ifdef __cplusplus
}
#endif

#endif
