// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2015-2019, Intel Corporation */
/*
 * config_reader_win.cpp -- config reader module definitions
 */
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <tchar.h>

#include "config_reader.hpp"
#include "queue.h"
#include "scenario.hpp"

#define SECTION_GLOBAL TEXT("global")
#define KEY_BENCHMARK TEXT("bench")
#define KEY_GROUP TEXT("group")

/*
 * Maximum section size according to MSDN documentation
 */
#define SIZEOF_SECTION 32767

#define NULL_LIST_EMPTY(x) (_tcslen(x) == 0)
#define NULL_LIST_NEXT(x) ((x) += (_tcslen(x) + 1))

#define KV_LIST_EMPTY(x) (_tcslen(x) == 0)
#define KV_FIRST(x)
#define KV_LIST_NEXT(x)                                                        \
	((x) += (_tcslen(x) + 1), (x) += (_tcslen(x) + 1),                     \
	 (x) = kv_list_skip_comment(x))

#define KV_LIST_KEY(x) (x)
#define KV_LIST_VALUE(x) ((x) + _tcslen(x) + 1)
#define KV_LIST_INIT(x) kv_list_init(x)

#define LIST LPTSTR
#define KV_LIST LPTSTR

/*
 * kv_list_skip_comment -- skip comment lines in ini file
 */
static KV_LIST
kv_list_skip_comment(KV_LIST list)
{
	while (list[0] == TEXT('#'))
		list += (_tcslen(list) + 1);
	return list;
}

/*
 * kv_list_init -- init KV list
 */
static KV_LIST
kv_list_init(LPTSTR list)
{
	list = kv_list_skip_comment(list);

	for (KV_LIST it = list; !KV_LIST_EMPTY(it); KV_LIST_NEXT(it)) {
		LPTSTR c = _tcsstr(it, TEXT("="));
		if (c == NULL)
			return NULL;

		*c = TEXT('\0');
	}
	return list;
}

/*
 * config_reader -- handle structure
 */
struct config_reader {
	LPTSTR lpFileName;
};

/*
 * config_reader_alloc -- allocate config reader
 */
struct config_reader *
config_reader_alloc(void)
{
	struct config_reader *cr = (struct config_reader *)malloc(sizeof(*cr));
	if (cr == NULL)
		return NULL;

	return cr;
}
/*
 * config_reader_read -- read config file
 */
int
config_reader_read(struct config_reader *cr, const char *fname)
{
	DWORD len = 0;
	LPTSTR buf = TEXT(" ");
	/* get the length of the full pathname incl. terminating null char */
	len = GetFullPathName((LPTSTR)fname, 0, buf, NULL);
	if (len == 0) {
		/* the function failed */
		return -1;
	} else {
		/* allocate a buffer large enough to store the pathname */
		LPTSTR buffer = (LPTSTR)malloc(len * sizeof(TCHAR));
		DWORD ret = GetFullPathName((LPTSTR)fname, len, buffer, NULL);

		if (_taccess(buffer, 0) != 0) {
			printf("%s", strerror(errno));
			return -1;
		}
		cr->lpFileName = (LPTSTR)buffer;
	}

	return 0;
}

/*
 * config_reader_free -- free config reader
 */
void
config_reader_free(struct config_reader *cr)
{
	free(cr);
}

/*
 * is_scenario -- (internal) return true if _name_ is scenario name
 *
 * This filters out the _global_ and _config_ sections.
 */
static int
is_scenario(LPTSTR name)
{
	return _tcscmp(name, SECTION_GLOBAL);
}

/*
 * is_argument -- (internal) return true if _name_ is argument name
 *
 * This filters out the _benchmark_ key.
 */
static int
is_argument(LPTSTR name)
{
	return _tcscmp(name, KEY_BENCHMARK) != 0 &&
		_tcscmp(name, KEY_GROUP) != 0;
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
	int ret = 0;

	TCHAR *sections = (TCHAR *)malloc(sizeof(TCHAR) * SIZEOF_SECTION);
	if (!sections)
		return -1;

	GetPrivateProfileSectionNames(sections, SIZEOF_SECTION, cr->lpFileName);

	if (NULL_LIST_EMPTY(sections)) {
		ret = -1;
		goto err_sections;
	}

	/*
	 * Check if global section is present and read it.
	 */
	TCHAR *global = (TCHAR *)malloc(sizeof(TCHAR) * SIZEOF_SECTION);
	if (!global)
		return -1;

	GetPrivateProfileSection(SECTION_GLOBAL, global, SIZEOF_SECTION,
				 cr->lpFileName);
	KV_LIST global_kv = KV_LIST_INIT(global);

	int has_global = !KV_LIST_EMPTY(global_kv);

	struct scenarios *s = scenarios_alloc();
	assert(NULL != s);
	if (!s) {
		ret = -1;
		goto err_gkeys;
	}

	LPTSTR global_group = NULL;
	for (KV_LIST it = global_kv; !KV_LIST_EMPTY(it); KV_LIST_NEXT(it)) {
		if (_tcscmp(KV_LIST_KEY(it), KEY_GROUP) == 0) {
			global_group = KV_LIST_VALUE(it);
			break;
		}
	}
	TCHAR *section;
	for (LPTSTR group_name = sections; !NULL_LIST_EMPTY(group_name);
	     group_name = NULL_LIST_NEXT(group_name)) {
		/*
		 * Check whether a group is a scenario
		 * or global section.
		 */
		if (!is_scenario(group_name))
			continue;

		/*
		 * Check for KEY_BENCHMARK which contains benchmark name.
		 * If not present the benchmark name is the same as the
		 * name of the section.
		 */
		section = (TCHAR *)malloc(sizeof(TCHAR) * SIZEOF_SECTION);
		if (!section)
			ret = -1;
		GetPrivateProfileSection(group_name, section, SIZEOF_SECTION,
					 cr->lpFileName);

		KV_LIST section_kv = KV_LIST_INIT(section);
		struct scenario *scenario = NULL;
		LPTSTR name = NULL;
		LPTSTR group = NULL;
		for (KV_LIST it = section_kv; !KV_LIST_EMPTY(it);
		     KV_LIST_NEXT(it)) {
			if (_tcscmp(KV_LIST_KEY(it), KEY_BENCHMARK) == 0) {
				name = KV_LIST_VALUE(it);
			}
			if (_tcscmp(KV_LIST_KEY(it), KEY_GROUP) == 0) {
				group = KV_LIST_VALUE(it);
			}
		}
		if (name == NULL) {
			scenario = scenario_alloc((const char *)group_name,
						  (const char *)group_name);
		} else {
			scenario = scenario_alloc((const char *)group_name,
						  (const char *)name);
		}
		assert(scenario != NULL);

		if (has_global) {
			/*
			 * Merge key/values from global section.
			 */
			for (KV_LIST it = global_kv; !KV_LIST_EMPTY(it);
			     KV_LIST_NEXT(it)) {
				LPTSTR key = KV_LIST_KEY(it);
				if (!is_argument(key))
					continue;

				LPTSTR value = KV_LIST_VALUE(it);
				assert(NULL != value);
				if (!value) {
					ret = -1;
					goto err_scenarios;
				}

				struct kv *kv = kv_alloc((const char *)key,
							 (const char *)value);
				assert(NULL != kv);

				if (!kv) {
					ret = -1;
					goto err_scenarios;
				}

				PMDK_TAILQ_INSERT_TAIL(&scenario->head, kv,
						       next);
			}
		}

		/* check for group name */
		if (group) {
			scenario_set_group(scenario, (const char *)group);
		} else if (global_group) {
			scenario_set_group(scenario,
					   (const char *)global_group);
		}

		for (KV_LIST it = section_kv; !KV_LIST_EMPTY(it);
		     KV_LIST_NEXT(it)) {
			LPTSTR key = KV_LIST_KEY(it);
			if (!is_argument(key))
				continue;

			LPTSTR value = KV_LIST_VALUE(it);
			assert(NULL != value);
			if (!value) {
				ret = -1;
				goto err_scenarios;
			}

			struct kv *kv = kv_alloc((const char *)key,
						 (const char *)value);
			assert(NULL != kv);

			if (!kv) {
				ret = -1;
				goto err_scenarios;
			}

			PMDK_TAILQ_INSERT_TAIL(&scenario->head, kv, next);
		}
		PMDK_TAILQ_INSERT_TAIL(&s->head, scenario, next);

		free(section);
	}
	*scenarios = s;

	free(global);
	free(sections);
	return 0;

err_scenarios:
	free(section);
	scenarios_free(s);
err_gkeys:
	free(global);
err_sections:
	free(sections);
	return ret;
}
