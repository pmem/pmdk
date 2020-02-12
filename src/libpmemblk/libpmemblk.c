// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2018, Intel Corporation */

/*
 * libpmemblk.c -- pmem entry points for libpmemblk
 */

#include <stdio.h>
#include <stdint.h>

#include "libpmemblk.h"
#include "ctl_global.h"

#include "pmemcommon.h"
#include "blk.h"

/*
 * The variable from which the config is directly loaded. The string
 * cannot contain any comments or extraneous white characters.
 */
#define BLK_CONFIG_ENV_VARIABLE "PMEMBLK_CONF"

/*
 * The variable that points to a config file from which the config is loaded.
 */
#define BLK_CONFIG_FILE_ENV_VARIABLE "PMEMBLK_CONF_FILE"

/*
 * blk_ctl_init_and_load -- (static) initializes CTL and loads configuration
 *	from env variable and file
 */
static int
blk_ctl_init_and_load(PMEMblkpool *pbp)
{
	LOG(3, "pbp %p", pbp);

	if (pbp != NULL && (pbp->ctl = ctl_new()) == NULL) {
		LOG(2, "!ctl_new");
		return -1;
	}

	char *env_config = os_getenv(BLK_CONFIG_ENV_VARIABLE);
	if (env_config != NULL) {
		if (ctl_load_config_from_string(pbp ? pbp->ctl : NULL,
				pbp, env_config) != 0) {
			LOG(2, "unable to parse config stored in %s "
				"environment variable",
				BLK_CONFIG_ENV_VARIABLE);
			goto err;
		}
	}

	char *env_config_file = os_getenv(BLK_CONFIG_FILE_ENV_VARIABLE);
	if (env_config_file != NULL && env_config_file[0] != '\0') {
		if (ctl_load_config_from_file(pbp ? pbp->ctl : NULL,
				pbp, env_config_file) != 0) {
			LOG(2, "unable to parse config stored in %s "
				"file (from %s environment variable)",
				env_config_file,
				BLK_CONFIG_FILE_ENV_VARIABLE);
			goto err;
		}
	}

	return 0;
err:
	if (pbp)
		ctl_delete(pbp->ctl);
	return -1;
}

/*
 * libpmemblk_init -- (internal) load-time initialization for blk
 *
 * Called automatically by the run-time loader.
 */
ATTR_CONSTRUCTOR
void
libpmemblk_init(void)
{
	ctl_global_register();

	if (blk_ctl_init_and_load(NULL))
		FATAL("error: %s", pmemblk_errormsg());

	common_init(PMEMBLK_LOG_PREFIX, PMEMBLK_LOG_LEVEL_VAR,
			PMEMBLK_LOG_FILE_VAR, PMEMBLK_MAJOR_VERSION,
			PMEMBLK_MINOR_VERSION);
	LOG(3, NULL);
}

/*
 * libpmemblk_fini -- libpmemblk cleanup routine
 *
 * Called automatically when the process terminates.
 */
ATTR_DESTRUCTOR
void
libpmemblk_fini(void)
{
	LOG(3, NULL);
	common_fini();
}

/*
 * pmemblk_check_versionU -- see if lib meets application version requirements
 */
#ifndef _WIN32
static inline
#endif
const char *
pmemblk_check_versionU(unsigned major_required, unsigned minor_required)
{
	LOG(3, "major_required %u minor_required %u",
			major_required, minor_required);

	if (major_required != PMEMBLK_MAJOR_VERSION) {
		ERR("libpmemblk major version mismatch (need %u, found %u)",
			major_required, PMEMBLK_MAJOR_VERSION);
		return out_get_errormsg();
	}

	if (minor_required > PMEMBLK_MINOR_VERSION) {
		ERR("libpmemblk minor version mismatch (need %u, found %u)",
			minor_required, PMEMBLK_MINOR_VERSION);
		return out_get_errormsg();
	}

	return NULL;
}

#ifndef _WIN32
/*
 * pmemblk_check_version -- see if lib meets application version requirements
 */
const char *
pmemblk_check_version(unsigned major_required, unsigned minor_required)
{
	return pmemblk_check_versionU(major_required, minor_required);
}
#else
/*
 * pmemblk_check_versionW -- see if lib meets application version requirements
 */
const wchar_t *
pmemblk_check_versionW(unsigned major_required, unsigned minor_required)
{
	if (pmemblk_check_versionU(major_required, minor_required) != NULL)
		return out_get_errormsgW();
	else
		return NULL;
}
#endif

/*
 * pmemblk_set_funcs -- allow overriding libpmemblk's call to malloc, etc.
 */
void
pmemblk_set_funcs(
		void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s))
{
	LOG(3, NULL);

	util_set_alloc_funcs(malloc_func, free_func, realloc_func, strdup_func);
}

/*
 * pmemblk_errormsgU -- return last error message
 */
#ifndef _WIN32
static inline
#endif
const char *
pmemblk_errormsgU(void)
{
	return out_get_errormsg();
}

#ifndef _WIN32
/*
 * pmemblk_errormsg -- return last error message
 */
const char *
pmemblk_errormsg(void)
{
	return pmemblk_errormsgU();
}
#else
/*
 * pmemblk_errormsgW -- return last error message as wchar_t
 */
const wchar_t *
pmemblk_errormsgW(void)
{
	return out_get_errormsgW();
}
#endif
