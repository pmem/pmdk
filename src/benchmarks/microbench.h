/*
 * Copyright 2019, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
/*
 * microbench.h -- micro-benchmarking instrumentation
 *
 * MBENCH_* is header-only micro-benchmarking instrumentation. How to add it to
 * the source code and how to use is described in the comment below.
 *
 * **Note:** It is not recommended to enable the micro-benchmarking
 * instrumentation permanently because it affects performance of the
 * instrumented code.
 *
 * Usage example:
 *
 * #define MBENCH_ENABLE
 * #include "microbench.h"
 *
 * void test() {
 *     MBENCH_START("probe_name")
 *     {
 *         do_sth();
 * #ifdef MBENCH_ENABLED
 *         MBENCH_IF
 *         {
 *             cleanup();
 *         }
 * #endif
 *     }
 *     MBENCH_STOP;
 * }
 *
 * Set MBENCH_LOG, MBENCH_PROBE and MBENCH_REPEAT environment variables prior
 * to run a test.
 * - MBENCH_LOG should point to a file for storing the results.
 * - MBENCH_PROBE is the name of the probe being tested ("probe_name" in the
 * above example).
 * - MBENCH_REPEAT should be a number the test is repeated between the two time
 * probes.
 *
 * The result is calculated as follows:
 *     time_diff = time_after_test - time_before_test;
 *     result = time_diff / MBENCH_REPEAT; // single operation time
 *
 * If you have a warm up phase, which you do not want to benchmark, you can hold
 * benchmarking for this phase and restore it afterwards e.g.:
 *
 * void main() {
 *     MBENCH_INIT;
 *     {
 *         MBENCH_HOLD;
 *         {
 *             test(); // warmup
 *         }
 *         MBENCH_RELEASE;
 *
 *         test();
 *     }
 *     MBENCH_FINI;
 * }
 *
 * Furthermore, the following accessory commands are available:
 * - MBENCH_IF condition which allows executing a code block conditionally. The
 * code block is executed if the probe is being tested. It is useful for
 * preparing for the next round of the micro-benchmarking loop. Note:
 *     - It is available only between MBENCH_START and MBENCH_STOP.
 *     - For performance reasons, it is required to put it between #ifdef
 *     MBENCH_ENABLED and #endif preprocessor directives.
 * - MBENCH_GET_REPEAT returns the number of the repeats set in the environment.
 * - MBENCH_PROBE_STRSTR allows checking if MBENCH_PROBE contains a specific
 * substring.
 * - MBENCH_INC allows increasing the specified variable by the value provided
 * using MBENCH_INC environment variable.
 *
 * void test2() {
 *     char *data = NULL;
 *     size_t repeat = MBENCH_GET_REPEAT;
 *     size_t index = 0;
 *
 *     if (MBENCH_PROBE_STRSTR("seq"))
 *         data = calloc(repeat, sizeof(char));
 *     else
 *         data = calloc(1, sizeof(char));
 *
 *     MBENCH_START("seq")
 *     {
 *         do_sth(&data[index]);
 *
 * #ifdef MBENCH_ENABLED
 *         MBENCH_IF
 *         {
 *             MBENCH_INC(index);
 *             index %= repeat;
 *         }
 * #endif
 *     }
 *     MBENCH_STOP;
 *
 *     MBENCH_START("in_situ")
 *     {
 *         do_sth(&data[index]);
 *     }
 *     MBENCH_STOP;
 *
 *     free(data);
 * }
 *
 * Note the test2 function compiled without MBENCH_ENABLED macro is equivalent
 * to the following source code without micro-benchmarking instrumentation:
 *
 * void test2() {
 *     char *data = NULL;
 *     size_t repeat = 0;
 *     size_t index = 0;
 *
 *     if (0)
 *         data = calloc(repeat, sizeof(char));
 *     else
 *         data = calloc(1, sizeof(char));
 *
 *     do_sth(&data[index]);
 *
 *     do_sth(&data[index]);
 *
 *     free(data);
 * }
 */

#ifndef MBENCH_H
#define MBENCH_H

#ifdef MBENCH_ENABLED

#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>

#define _MBENCH_LOG_ENV		"MBENCH_LOG"	/* log file */
#define _MBENCH_REPEAT_ENV	"MBENCH_REPEAT"	/* number of test repetitions */
#define _MBNECH_REPEAT_DEF	1

#define _MBENCH_PROBE_ENV	"MBENCH_PROBE"	/* probe name */
#define _MBENCH_MAX_PROBE_LEN	32

#define _MBENCH_INC_ENV		"MBENCH_INC"	/* counter increase diff */
#define _MBNECH_INC_DEF		1

#define _MBENCH_OFF_INC_ENV	"MBENCH_OFF_INC" /* offset increase diff */
#define _MBENCH_OFF_INC_DEF	4096

#define _MBENCH_HOLD_ENV	"MBENCH_HOLD"	/* store hold status */
#define _MBENCH_HOLD_VAL	"h"

#define _MBENCH_PREFIX		"MBENCH_PROBE: "

#define _MBENCH_TRUE	1
#define _MBENCH_FALSE	0

struct _mbench_params_t
{
	long repeat;
	int do_bench;
	char *probe;
	long inc;
	long off_inc;
	char *log_name;
};

struct _mbench_state_t
{
	struct _mbench_params_t params;

	int initialized;

	FILE *log;

	/* start and stop time */
	struct timespec t1;
	struct timespec t2;
};

#define _MBENCH_PARAMS_DEFAULT \
{ \
	_MBNECH_REPEAT_DEF,	\
	_MBENCH_TRUE,		/* do_bench */ \
	NULL,			/* probes */ \
	_MBNECH_INC_DEF,	/* inc */ \
	_MBENCH_OFF_INC_DEF,	/* off_inc */ \
	NULL			/* log_name */ \
} \

#define _MBENCH_NO_DEFAULT	(-1)

static struct _mbench_state_t mbench_state = {{0}};

/*
 * _mbench_env2long -- convert environment variable to long
 */
static inline long
_mbench_env2long(const char *env_name, long default_val)
{
	char *env_str = getenv(env_name);
	if (env_str) {
		errno = 0;
		long val = strtol(env_str, NULL, 10);
		if (val == LONG_MIN || val == LONG_MAX)
			return -1;
		if (val == 0 && errno == EINVAL)
			return -1;
		return val;
	}

	return default_val;
}

#define _MBENCH_STATE_STR "Micro-benchmark state: "

/*
 * _mbench_init -- initialize micro-benchmark
 */
static inline void
_mbench_init(struct _mbench_state_t *state)
{
	assert(state->initialized == 0);

	struct _mbench_params_t *params = &state->params;
	struct _mbench_params_t pdefault = _MBENCH_PARAMS_DEFAULT;
	memcpy(params, &pdefault, sizeof(pdefault));

	/* get a number of repeats and the enabled probe */
	params->repeat = _mbench_env2long(_MBENCH_REPEAT_ENV,
			_MBNECH_REPEAT_DEF);
	params->probe = getenv(_MBENCH_PROBE_ENV);

	/* check if parameters does not disable benchmarking */
	if (params->repeat < 1 || !params->probe) {
		params->do_bench = _MBENCH_FALSE;
		params->repeat = 1;
	}

	if (!params->do_bench) {
		fprintf(stdout, _MBENCH_STATE_STR "disabled\n");
		state->initialized = _MBENCH_TRUE;
		return;
	} else
		fprintf(stdout, _MBENCH_STATE_STR "enabled\n");

	/* process a counter and an offset incrementation values */
	params->inc = _mbench_env2long(_MBENCH_INC_ENV, _MBNECH_INC_DEF);
	params->off_inc = _mbench_env2long(_MBENCH_OFF_INC_ENV,
			_MBENCH_OFF_INC_DEF);

	/* open the log file */
	params->log_name = getenv(_MBENCH_LOG_ENV);
	if (params->log_name)
		state->log = fopen(params->log_name, "a");
	else
		state->log = stdout;

	state->initialized = _MBENCH_TRUE;
}

/*
 * _mbench_fini -- finalize microbenchmark
 */
static inline void
_mbench_fini(struct _mbench_state_t *state)
{
	/* close the log file */
	if (state->params.log_name) {
		assert(state->log != stdout);

		fclose(state->log);
	}

	memset(state, 0, sizeof(*state));
}

/*
 * _mbench_probe_is_enabled -- check if the probe is enabled
 */
static inline int
_mbench_probe_is_enabled(struct _mbench_params_t *params, const char *probe)
{
	if (!params->do_bench)
		return _MBENCH_FALSE;

	if (getenv(_MBENCH_HOLD_ENV))
		return _MBENCH_FALSE;

	assert(params->probe);
	return strncmp(params->probe, probe, _MBENCH_MAX_PROBE_LEN) == 0;
}

/*
 * _mbench_probe_strstr -- check if the enabled probe name contains string
 */
static inline int
_mbench_probe_strstr(struct _mbench_params_t *params, const char *probe)
{
	if (!params->do_bench)
		return _MBENCH_FALSE;

	assert(params->probe);
	return strstr(params->probe, probe) != NULL;
}

/*
 * _mbench_get_repeat -- return number of repeats
 */
static inline int
_mbench_get_repeat(struct _mbench_params_t *params)
{
	if (!params->do_bench)
		return -1;

	return params->repeat;
}

/*
 * _mbench_print_params -- print effective configuration
 */
static inline void
_mbench_print_params(FILE *log, struct _mbench_params_t *params)
{
	fprintf(log, "\nEffective configuration:\n");
	fprintf(log, "repeat  \t= %ld\n", params->repeat);
	fprintf(log, "do_bench\t= %s\n", params->do_bench ? "yes" : "no");
	fprintf(log, "probe   \t= %s\n",
			params->probe ? params->probe : "NULL");
	fprintf(log, "inc     \t= %ld\n", params->inc);
	fprintf(log, "off_inc \t= %ld\n", params->off_inc);
	fprintf(log, "log_name\t= %s\n", params->log_name ? params->log_name
			: "stdout");
}

/*
 * _mbench_gettime -- get a timer value
 */
static inline void
_mbench_gettime(struct timespec *ts)
{
	os_clock_gettime(CLOCK_MONOTONIC, ts);
}

/*
 * _mbench_time_diff -- calculate time diff in nanoseconds
 */
static inline int64_t
_mbench_time_diff(struct timespec *t1, struct timespec *t2)
{
	const int64_t nspecpsec = 1000000000;
	return (t2->tv_sec - t1->tv_sec) * nspecpsec +
			t2->tv_nsec - t1->tv_nsec;
}

/*
 * _mbench_start --  verify if probe is enabled and if it does initialize
 * benchmarking
 */
static inline int
_mbench_start(struct _mbench_state_t *state, const char *probe)
{
	struct _mbench_params_t *params = &state->params;

	if (_mbench_probe_is_enabled(params, probe) == _MBENCH_FALSE)
		return _MBENCH_FALSE;

	_mbench_print_params(state->log, params);
	return _MBENCH_TRUE;
}

/*
 * _mbench_stop -- stop benchmark
 */
static inline void
_mbench_stop(struct _mbench_state_t *state, long repeat)
{
	int64_t diff = _mbench_time_diff(&state->t1, &state->t2);
	diff /= repeat;
	fprintf(state->log, "\n%s [nsec]: %" PRId64 "\n",
			state->params.probe, diff);
}

/*
 * _mbench_hold -- hold all micro-benchmark looping till _mbench_release
 *
 * It supports nesting.
 */
static inline void
_mbench_hold()
{
	char *hold_old = getenv(_MBENCH_HOLD_ENV);
	if (!hold_old) {
		/* add first hold */
		if (setenv(_MBENCH_HOLD_ENV, _MBENCH_HOLD_VAL, 0))
			assert(0);
	} else {
		/* add subsequent hold level */
		size_t hold_old_len = strlen(hold_old);
		char *hold_new = (char *)malloc(
				sizeof(char) * (hold_old_len + 2));
		strcpy(hold_new, hold_old);
		strcat(hold_new, _MBENCH_HOLD_VAL);
		if (setenv(_MBENCH_HOLD_ENV, hold_new, 1))
			assert(0);
		free(hold_new);
	}
}

/*
 * _mbench_release -- release micro-benchmark looping after _mbench_hold
 */
static inline void
_mbench_release()
{
	char *hold_old = getenv(_MBENCH_HOLD_ENV);
	if (hold_old) {
		size_t hold_old_len = strlen(hold_old);
		if (hold_old_len == 1) {
			/* remove the last hold level - final release */
			if (unsetenv(_MBENCH_HOLD_ENV))
				assert(0);
		} else { \
			/* remove a hold level - microbench still suspended */
			char *hold_new = strndup(hold_old, hold_old_len - 1);
			if (setenv(_MBENCH_HOLD_ENV, hold_new, 1))
				assert(0);
			free(hold_new);
		}
	}
}

#define MBENCH_INIT() _mbench_init(&mbench_state)

#define MBENCH_FINI() _mbench_fini(&mbench_state)

/* micro-benchmark loop entry point */
#define MBENCH_START(name) \
{ \
	assert(mbench_state.initialized != 0); \
	static __attribute__((used)) const char *mbench_probe_str = \
			_MBENCH_PREFIX name; \
	\
	long _mb_repeat = mbench_state.params.repeat; \
	int _mb_do = _MBENCH_TRUE; \
	if (_mbench_start(&mbench_state, name) == _MBENCH_FALSE) {\
		_mb_repeat = 1; \
		_mb_do = _MBENCH_FALSE; \
	} else \
		_mbench_gettime(&mbench_state.t1); \
	for (long _mb_i = 0; _mb_i < _mb_repeat; ++_mb_i)

/* micro-benchmark loop exit point */
#define MBENCH_STOP \
	if (_mb_do) { \
		_mbench_gettime(&mbench_state.t2); \
		_mbench_stop(&mbench_state, _mb_repeat); \
	} \
}

#define MBENCH_IF \
	if (likely(_mb_do))

#define MBENCH_INC(val) \
	val += mbench_state.params.inc

#define MBENCH_PROBE_STRSTR(name) \
	_mbench_probe_strstr(&mbench_state.params, name)

#define MBENCH_GET_REPEAT _mbench_get_repeat(&mbench_state.params)

/* hold all micro-benchmark looping till MBENCH_RELEASE */
#define MBENCH_HOLD() _mbench_hold()

/* release micro-benchmark looping after MBENCH_HOLD */
#define MBENCH_RELEASE() _mbench_release()

#else

#define MBENCH_INIT() {}
#define MBENCH_FINI() {}

#define MBENCH_START(arg) {}
#define MBENCH_STOP

#define MBENCH_PROBE_STRSTR(arg) 0
#define MBENCH_GET_REPEAT 0

#define MBENCH_HOLD() {}
#define MBENCH_RELEASE() {}

#endif

#endif /* MBENCH_H */
