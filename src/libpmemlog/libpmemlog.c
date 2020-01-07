// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2014-2018, Intel Corporation */

/*
 * libpmemlog.c -- pmem entry points for libpmemlog
 */

#include <stdio.h>
#include <stdint.h>

#include "libpmemlog.h"
#include "ctl_global.h"

#include "pmemcommon.h"
#include "log.h"

/*
 * The variable from which the config is directly loaded. The string
 * cannot contain any comments or extraneous white characters.
 */
#define LOG_CONFIG_ENV_VARIABLE "PMEMLOG_CONF"

/*
 * The variable that points to a config file from which the config is loaded.
 */
#define LOG_CONFIG_FILE_ENV_VARIABLE "PMEMLOG_CONF_FILE"

/*
 * log_ctl_init_and_load -- (static) initializes CTL and loads configuration
 *	from env variable and file
 */
static int
log_ctl_init_and_load(PMEMlogpool *plp)
{
	LOG(3, "plp %p", plp);

	if (plp != NULL && (plp->ctl = ctl_new()) == NULL) {
		LOG(2, "!ctl_new");
		return -1;
	}

	char *env_config = os_getenv(LOG_CONFIG_ENV_VARIABLE);
	if (env_config != NULL) {
		if (ctl_load_config_from_string(plp ? plp->ctl : NULL,
				plp, env_config) != 0) {
			LOG(2, "unable to parse config stored in %s "
				"environment variable",
				LOG_CONFIG_ENV_VARIABLE);
			goto err;
		}
	}

	char *env_config_file = os_getenv(LOG_CONFIG_FILE_ENV_VARIABLE);
	if (env_config_file != NULL && env_config_file[0] != '\0') {
		if (ctl_load_config_from_file(plp ? plp->ctl : NULL,
				plp, env_config_file) != 0) {
			LOG(2, "unable to parse config stored in %s "
				"file (from %s environment variable)",
				env_config_file,
				LOG_CONFIG_FILE_ENV_VARIABLE);
			goto err;
		}
	}

	return 0;
err:
	if (plp)
		ctl_delete(plp->ctl);
	return -1;
}

/*
 * log_init -- load-time initialization for log
 *
 * Called automatically by the run-time loader.
 */
ATTR_CONSTRUCTOR
void
libpmemlog_init(void)
{
	ctl_global_register();

	if (log_ctl_init_and_load(NULL))
		FATAL("error: %s", pmemlog_errormsg());

	common_init(PMEMLOG_LOG_PREFIX, PMEMLOG_LOG_LEVEL_VAR,
			PMEMLOG_LOG_FILE_VAR, PMEMLOG_MAJOR_VERSION,
			PMEMLOG_MINOR_VERSION);
	LOG(3, NULL);
}

/*
 * libpmemlog_fini -- libpmemlog cleanup routine
 *
 * Called automatically when the process terminates.
 */
ATTR_DESTRUCTOR
void
libpmemlog_fini(void)
{
	LOG(3, NULL);
	common_fini();
}

/*
 * pmemlog_check_versionU -- see if lib meets application version requirements
 */
#ifndef _WIN32
static inline
#endif
const char *
pmemlog_check_versionU(unsigned major_required, unsigned minor_required)
{
	LOG(3, "major_required %u minor_required %u",
			major_required, minor_required);

	if (major_required != PMEMLOG_MAJOR_VERSION) {
		ERR("libpmemlog major version mismatch (need %u, found %u)",
			major_required, PMEMLOG_MAJOR_VERSION);
		return out_get_errormsg();
	}

	if (minor_required > PMEMLOG_MINOR_VERSION) {
		ERR("libpmemlog minor version mismatch (need %u, found %u)",
			minor_required, PMEMLOG_MINOR_VERSION);
		return out_get_errormsg();
	}

	return NULL;
}

#ifndef _WIN32
/*
 * pmemlog_check_version -- see if lib meets application version requirements
 */
const char *
pmemlog_check_version(unsigned major_required, unsigned minor_required)
{
	return pmemlog_check_versionU(major_required, minor_required);
}
#else
/*
 * pmemlog_check_versionW -- see if lib meets application version requirements
 */
const wchar_t *
pmemlog_check_versionW(unsigned major_required, unsigned minor_required)
{
	if (pmemlog_check_versionU(major_required, minor_required) != NULL)
		return out_get_errormsgW();
	else
		return NULL;
}
#endif

/*
 * pmemlog_set_funcs -- allow overriding libpmemlog's call to malloc, etc.
 */
void
pmemlog_set_funcs(
		void *(*malloc_func)(size_t size),
		void (*free_func)(void *ptr),
		void *(*realloc_func)(void *ptr, size_t size),
		char *(*strdup_func)(const char *s))
{
	LOG(3, NULL);

	util_set_alloc_funcs(malloc_func, free_func, realloc_func, strdup_func);
}

/*
 * pmemlog_errormsgU -- return last error message
 */
#ifndef _WIN32
static inline
#endif
const char *
pmemlog_errormsgU(void)
{
	return out_get_errormsg();
}

#ifndef _WIN32
/*
 * pmemlog_errormsg -- return last error message
 */
const char *
pmemlog_errormsg(void)
{
	return pmemlog_errormsgU();
}
#else
/*
 * pmemlog_errormsgW -- return last error message as wchar_t
 */
const wchar_t *
pmemlog_errormsgW(void)
{
	return out_get_errormsgW();
}

#endif
