/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright 2015-2020, Intel Corporation */
/*
 * clo.hpp -- command line options module declarations
 */
int benchmark_clo_parse(int argc, char *argv[], struct benchmark_clo *clos,
			ssize_t nclo, struct clo_vec *clovec);
int benchmark_clo_parse_scenario(struct scenario *scenario,
				 struct benchmark_clo *clos, size_t nclo,
				 struct clo_vec *clovec);
const char *benchmark_clo_str(struct benchmark_clo *clo, void *args,
			      size_t size);
int clo_get_scenarios(int argc, char *argv[],
		      struct scenarios *available_scenarios,
		      struct scenarios *found_scenarios);
int benchmark_override_clos_in_scenario(struct scenario *scenario, int argc,
					char *argv[],
					struct benchmark_clo *clos, int nclos);
