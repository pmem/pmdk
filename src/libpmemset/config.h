/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020, Intel Corporation */

/*
 * config.h -- internal definitions for pmemset_config
 */
#ifndef PMEMSET_CONFIG_H
#define PMEMSET_CONFIG_H

struct pmemset_config {
	char stub;
};

void pmemset_config_init(struct pmemset_config *cfg);

#endif /* PMEMSET_CONFIG_H */
