// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */
/*
 * config_reader.cpp -- config reader module definitions
 */
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <glib.h>
#include <sys/queue.h>

#include "config_reader.hpp"
#include "scenario.hpp"

#define SECTION_GLOBAL "global"
#define KEY_BENCHMARK "bench"
#define KEY_GROUP "group"

/*
 * config_reader -- handle structure
 */
struct config_reader {
	GKeyFile *key_file;
};

/*
 * config_reader_alloc -- allocate config reader
 */
struct config_reader *
config_reader_alloc(void)
{
	struct config_reader *cr = (struct config_reader *)malloc(sizeof(*cr));
	assert(cr != nullptr);

	cr->key_file = g_key_file_new();
	if (!cr->key_file)
		goto err;

	return cr;

err:
	free(cr);
	return nullptr;
}

/*
 * config_reader_read -- read config file
 */
int
config_reader_read(struct config_reader *cr, const char *fname)
{
	if (g_key_file_load_from_file(cr->key_file, fname, G_KEY_FILE_NONE,
				      nullptr) != TRUE)
		return -1;

	return 0;
}

/*
 * config_reader_free -- free config reader
 */
void
config_reader_free(struct config_reader *cr)
{
	g_key_file_free(cr->key_file);
	free(cr);
}

/*
 * is_scenario -- (internal) return true if _name_ is scenario name
 *
 * This filters out the _global_ and _config_ sections.
 */
static int
is_scenario(const char *name)
{
	return strcmp(name, SECTION_GLOBAL);
}

/*
 * is_argument -- (internal) return true if _name_ is argument name
 *
 * This filters out the _benchmark_ key.
 */
static int
is_argument(const char *name)
{
	return strcmp(name, KEY_BENCHMARK) != 0 && strcmp(name, KEY_GROUP) != 0;
}

/*
 * config_reader_get_scenarios -- return scenarios from config file
 *
 * This function reads the config file and returns a list of scenarios.
 * Each scenario contains a list of key/value arguments.
 * The scenario's arguments are merged with arguments from global section.
 */
int
config_reader_get_scenarios(struct config_reader *cr,
			    struct scenarios **scenarios)
{
	/*
	 * Read all groups.
	 * The config file must have at least one group, otherwise
	 * it is considered as invalid.
	 */
	gsize ngroups;
	gsize g;
	gchar **groups = g_key_file_get_groups(cr->key_file, &ngroups);
	assert(nullptr != groups);
	if (!groups)
		return -1;

	/*
	 * Check if global section is present and read keys from it.
	 */
	int ret = 0;
	int has_global =
		g_key_file_has_group(cr->key_file, SECTION_GLOBAL) == TRUE;
	gsize ngkeys;
	gchar **gkeys = nullptr;
	struct scenarios *s;
	if (has_global) {
		gkeys = g_key_file_get_keys(cr->key_file, SECTION_GLOBAL,
					    &ngkeys, nullptr);
		assert(nullptr != gkeys);
		if (!gkeys) {
			ret = -1;
			goto err_groups;
		}
	}

	s = scenarios_alloc();
	assert(nullptr != s);
	if (!s) {
		ret = -1;
		goto err_gkeys;
	}

	for (g = 0; g < ngroups; g++) {
		/*
		 * Check whether a group is a scenario
		 * or global section.
		 */
		if (!is_scenario(groups[g]))
			continue;

		/*
		 * Check for KEY_BENCHMARK which contains benchmark name.
		 * If not present the benchmark name is the same as the
		 * name of the section.
		 */
		struct scenario *scenario = nullptr;
		if (g_key_file_has_key(cr->key_file, groups[g], KEY_BENCHMARK,
				       nullptr) == FALSE) {
			scenario = scenario_alloc(groups[g], groups[g]);
			assert(scenario != nullptr);
		} else {
			gchar *benchmark =
				g_key_file_get_value(cr->key_file, groups[g],
						     KEY_BENCHMARK, nullptr);
			assert(benchmark != nullptr);
			if (!benchmark) {
				ret = -1;
				goto err_scenarios;
			}
			scenario = scenario_alloc(groups[g], benchmark);
			assert(scenario != nullptr);
			free(benchmark);
		}

		gsize k;
		if (has_global) {
			/*
			 * Merge key/values from global section.
			 */
			for (k = 0; k < ngkeys; k++) {
				if (g_key_file_has_key(cr->key_file, groups[g],
						       gkeys[k],
						       nullptr) == TRUE)
					continue;

				if (!is_argument(gkeys[k]))
					continue;

				char *value = g_key_file_get_value(
					cr->key_file, SECTION_GLOBAL, gkeys[k],
					nullptr);
				assert(nullptr != value);
				if (!value) {
					ret = -1;
					goto err_scenarios;
				}

				struct kv *kv = kv_alloc(gkeys[k], value);
				assert(nullptr != kv);

				free(value);
				if (!kv) {
					ret = -1;
					goto err_scenarios;
				}

				PMDK_TAILQ_INSERT_TAIL(&scenario->head, kv,
						       next);
			}
		}

		/* check for group name */
		if (g_key_file_has_key(cr->key_file, groups[g], KEY_GROUP,
				       nullptr) != FALSE) {
			gchar *group = g_key_file_get_value(
				cr->key_file, groups[g], KEY_GROUP, nullptr);
			assert(group != nullptr);
			scenario_set_group(scenario, group);
		} else if (g_key_file_has_key(cr->key_file, SECTION_GLOBAL,
					      KEY_GROUP, nullptr) != FALSE) {
			gchar *group = g_key_file_get_value(cr->key_file,
							    SECTION_GLOBAL,
							    KEY_GROUP, nullptr);
			scenario_set_group(scenario, group);
		}

		gsize nkeys;
		gchar **keys = g_key_file_get_keys(cr->key_file, groups[g],
						   &nkeys, nullptr);
		assert(nullptr != keys);
		if (!keys) {
			ret = -1;
			goto err_scenarios;
		}

		/*
		 * Read key/values from the scenario's section.
		 */
		for (k = 0; k < nkeys; k++) {
			if (!is_argument(keys[k]))
				continue;
			char *value = g_key_file_get_value(
				cr->key_file, groups[g], keys[k], nullptr);
			assert(nullptr != value);
			if (!value) {
				ret = -1;
				g_strfreev(keys);
				goto err_scenarios;
			}

			struct kv *kv = kv_alloc(keys[k], value);
			assert(nullptr != kv);

			free(value);
			if (!kv) {
				g_strfreev(keys);
				ret = -1;
				goto err_scenarios;
			}

			PMDK_TAILQ_INSERT_TAIL(&scenario->head, kv, next);
		}

		g_strfreev(keys);

		PMDK_TAILQ_INSERT_TAIL(&s->head, scenario, next);
	}

	g_strfreev(gkeys);
	g_strfreev(groups);

	*scenarios = s;
	return 0;
err_scenarios:
	scenarios_free(s);
err_gkeys:
	g_strfreev(gkeys);
err_groups:
	g_strfreev(groups);
	return ret;
}
