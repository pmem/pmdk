/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2020, Intel Corporation */

/*
 * config.h -- internal definitions for pmemset_config
 */
#ifndef PMEMSET_CONFIG_H
#define PMEMSET_CONFIG_H

void pmemset_config_init(struct pmemset_config *cfg);
int pmemset_config_duplicate(struct pmemset_config **cfg_out,
				struct pmemset_config *cfg_in);

#endif /* PMEMSET_CONFIG_H */
