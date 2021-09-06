/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */
/*
 * config_reader.hpp -- config reader module declarations
 */
struct config_reader;

struct config_reader *config_reader_alloc(void);
int config_reader_read(struct config_reader *cr, const char *fname);
void config_reader_free(struct config_reader *cr);
int config_reader_get_scenarios(struct config_reader *cr,
				struct scenarios **scenarios);
