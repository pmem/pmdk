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
 * **Note:** It is not recommended to add the micro-benchmarking
 * instrumentation permanently to the code base.
 *
 * Usage example:
 *
 * #define MBENCH_ENABLE
 * #include "microbench.h"
 *
 * void test() {
 *     MBENCH("probe_name", {
 *         do_sth();
 *         if (MBENCH_STARTED) {
 *             cleanup();
 *         }
 *     });
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
 *     MBENCH_INIT();
 *
 *     MBENCH_HOLD();
 *     test(); // warmup
 *     MBENCH_RELEASE();
 *
 *     test();
 *     MBENCH_FINI();
 * }
 *
 * Furthermore, the following accessory commands are available:
 * - MBENCH_STARTED tests if the probe is being tested. It is useful for
 * preparing for the next round of the micro-benchmarking loop.
 * - MBENCH_GET_REPEAT returns the number of the repeats set in the environment.
 * - MBENCH_PROBE_STRSTR allows checking if MBENCH_PROBE contains a specific
 * substring.
 * - MBENCH_INC allows increasing the specified variable by the value provided
 * using MBENCH_INC environment variable.
 *
 * void test2() {
 *     char *data = NULL;
 *     size_t repeat = MBENCH_GET_REPEAT();
 *     size_t index = 0;
 *
 *     if (MBENCH_PROBE_STRSTR("seq"))
 *         data = calloc(repeat, sizeof(char));
 *     else
 *         data = calloc(1, sizeof(char));
 *
 *     MBENCH("seq", {
 *         do_sth(&data[index]);
 *
 *         if (MBENCH_STARTED) {
 *             MBENCH_INC(index);
 *             index %= repeat;
 *         }
 *     });
 *
 *     MBENCH("in_situ", {
 *         do_sth(&data[index]);
 *     });
 *
 *     free(data);
 * }
 *
 */

#ifndef MICROBENCH_H
#define MICROBENCH_H

#ifdef MBENCH_ENABLED

#include <assert.h>
#include <stdlib.h>
#include <inttypes.h>

#define MICROBENCH_LOG_ENV	 "MBENCH_LOG"	  /* log file */
#define MICROBENCH_REPEAT_ENV	 "MBENCH_REPEAT"  /* # of test repetitions */
#define MICROBENCH_REPEAT_DEF	 (1)

#define MICROBENCH_PROBE_ENV	 "MBENCH_PROBE"	  /* probe name */
#define MICROBENCH_MAX_PROBE_LEN (32)

#define MICROBENCH_INC_ENV	 "MBENCH_INC"	  /* counter increase diff */
#define MICROBENCH_INC_DEF	 (1)

#define MICROBENCH_OFF_INC_ENV	 "MBENCH_OFF_INC" /* offset increase diff */
#define MICROBENCH_OFF_INC_DEF	 (4096)

#define MICROBENCH_HOLD_ENV	 "MBENCH_HOLD"	 /* store hold status */
#define MICROBENCH_HOLD_VAL	 "h"

#define MICROBENCH_PREFIX	 "MBENCH_PROBE: "

#define MICROBENCH_TRUE		 (1)
#define MICROBENCH_FALSE	 (0)

struct microbench_params_t
{
	long repeat;
	int do_bench;
	char *probe;
	long inc;
	long off_inc;
	char *log_name;
};

struct microbench_run_time_t
{
	long repeat;
	int started;

	/* start and stop time */
	struct timespec t1;
	struct timespec t2;
};

struct microbench_state_t
{
	struct microbench_params_t params;

	int initialized;
	FILE *log;

	struct microbench_run_time_t rt;
};

#define MICROBENCH_PARAMS_DEFAULT \
{ \
	MICROBENCH_REPEAT_DEF,	\
	MICROBENCH_TRUE,		/* do_bench */ \
	NULL,			/* probes */ \
	MICROBENCH_INC_DEF,	/* inc */ \
	MICROBENCH_OFF_INC_DEF,	/* off_inc */ \
	NULL			/* log_name */ \
} \

#define MICROBENCH_NO_DEFAULT	(-1)

static struct microbench_state_t mbench_state = {{0}};

/*
 * microbench_env2long -- convert environment variable to long
 */
static inline long
microbench_env2long(const char *env_name, long default_val)
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

#define MICROBENCH_STATE_STR "Micro-benchmark state: "

/*
 * microbench_init -- initialize micro-benchmark
 */
static inline void
microbench_init(struct microbench_state_t *state)
{
	assert(state->initialized == 0);

	struct microbench_params_t *params = &state->params;
	struct microbench_params_t pdefault = MICROBENCH_PARAMS_DEFAULT;
	memcpy(params, &pdefault, sizeof(pdefault));

	/* get a number of repeats and the enabled probe */
	params->repeat = microbench_env2long(MICROBENCH_REPEAT_ENV,
			MICROBENCH_REPEAT_DEF);
	params->probe = getenv(MICROBENCH_PROBE_ENV);

	/* check if parameters does not disable benchmarking */
	if (params->repeat < 1 || !params->probe) {
		params->do_bench = MICROBENCH_FALSE;
		params->repeat = 1;
	}

	if (!params->do_bench) {
		fprintf(stdout, MICROBENCH_STATE_STR "disabled\n");
		state->initialized = MICROBENCH_TRUE;
		return;
	} else
		fprintf(stdout, MICROBENCH_STATE_STR "enabled\n");

	/* process a counter and an offset incrementation values */
	params->inc = microbench_env2long(
			MICROBENCH_INC_ENV, MICROBENCH_INC_DEF);
	params->off_inc = microbench_env2long(MICROBENCH_OFF_INC_ENV,
			MICROBENCH_OFF_INC_DEF);

	/* open the log file */
	params->log_name = getenv(MICROBENCH_LOG_ENV);
	if (params->log_name)
		state->log = fopen(params->log_name, "a");
	else
		state->log = stdout;

	state->initialized = MICROBENCH_TRUE;
}

/*
 * microbench_fini -- finalize microbenchmark
 */
static inline void
microbench_fini(struct microbench_state_t *state)
{
	/* close the log file */
	if (state->params.log_name) {
		assert(state->log != stdout);

		fclose(state->log);
	}

	memset(state, 0, sizeof(*state));
}

/*
 * microbench_probe_is_enabled -- check if the probe is enabled
 */
static inline int
microbench_probe_is_enabled(struct microbench_params_t *params,
		const char *probe)
{
	if (!params->do_bench)
		return MICROBENCH_FALSE;

	if (getenv(MICROBENCH_HOLD_ENV))
		return MICROBENCH_FALSE;

	assert(params->probe);
	return strncmp(params->probe, probe, MICROBENCH_MAX_PROBE_LEN) == 0;
}

/*
 * microbench_probe_strstr -- check if the enabled probe name contains string
 */
static inline int
microbench_probe_strstr(struct microbench_params_t *params, const char *probe)
{
	if (!params->do_bench)
		return MICROBENCH_FALSE;

	assert(params->probe);
	return strstr(params->probe, probe) != NULL;
}

/*
 * microbench_get_repeat -- return number of repeats
 */
static inline int
microbench_get_repeat(struct microbench_params_t *params)
{
	if (!params->do_bench)
		return -1;

	return params->repeat;
}

/*
 * microbench_print_params -- print effective configuration
 */
static inline void
microbench_print_params(FILE *log, struct microbench_params_t *params)
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
 * microbench_gettime -- get a timer value
 */
static inline void
microbench_gettime(struct timespec *ts)
{
	os_clock_gettime(CLOCK_MONOTONIC, ts);
}

/*
 * microbench_time_diff -- calculate time diff in nanoseconds
 */
static inline int64_t
microbench_time_diff(struct timespec *t1, struct timespec *t2)
{
	const int64_t nspecpsec = 1000000000;
	return (t2->tv_sec - t1->tv_sec) * nspecpsec +
			t2->tv_nsec - t1->tv_nsec;
}

/*
 * microbench_start --  verify if probe is enabled and if it does initialize
 * benchmarking
 */
static inline void
microbench_start(struct microbench_state_t *state, const char *probe)
{
	assert(state->initialized != 0);
	struct microbench_params_t *params = &state->params;

	state->rt.started = microbench_probe_is_enabled(params, probe);

	if (state->rt.started == MICROBENCH_TRUE) {
		microbench_print_params(state->log, params);
		state->rt.repeat = state->params.repeat;
	} else {
		state->rt.repeat = 1;
	}
}

/*
 * microbench_stop -- stop benchmark
 */
static inline void
microbench_stop(struct microbench_state_t *state)
{
	int64_t diff = microbench_time_diff(&state->rt.t1, &state->rt.t2);
	diff /= state->rt.repeat;
	fprintf(state->log, "\n%s [nsec]: %" PRId64 "\n",
			state->params.probe, diff);

	state->rt.started = MICROBENCH_FALSE;
}

/*
 * microbench_hold -- hold all micro-benchmark looping till microbench_release
 *
 * It supports nesting.
 */
static inline void
microbench_hold()
{
	char *hold_old = getenv(MICROBENCH_HOLD_ENV);
	if (!hold_old) {
		/* add first hold */
		if (setenv(MICROBENCH_HOLD_ENV, MICROBENCH_HOLD_VAL, 0))
			assert(0);
	} else {
		/* add subsequent hold level */
		size_t hold_old_len = strlen(hold_old);
		char *hold_new = (char *)malloc(
				sizeof(char) * (hold_old_len + 2));
		strcpy(hold_new, hold_old);
		strcat(hold_new, MICROBENCH_HOLD_VAL);
		if (setenv(MICROBENCH_HOLD_ENV, hold_new, 1))
			assert(0);
		free(hold_new);
	}
}

/*
 * microbench_release -- release micro-benchmark looping after microbench_hold
 */
static inline void
microbench_release()
{
	char *hold_old = getenv(MICROBENCH_HOLD_ENV);
	if (hold_old) {
		size_t hold_old_len = strlen(hold_old);
		if (hold_old_len == 1) {
			/* remove the last hold level - final release */
			if (unsetenv(MICROBENCH_HOLD_ENV))
				assert(0);
		} else { \
			/* remove a hold level - microbench still suspended */
			char *hold_new = strndup(hold_old, hold_old_len - 1);
			if (setenv(MICROBENCH_HOLD_ENV, hold_new, 1))
				assert(0);
			free(hold_new);
		}
	}
}

/*
 * recommended entry points start here
 */

#define MBENCH_INIT() microbench_init(&mbench_state)

#define MBENCH_FINI() microbench_fini(&mbench_state)

#define MBENCH(name, code_block) \
{ \
	static __attribute__((used)) const char *mbench_probe_str = \
			MICROBENCH_PREFIX name; \
	\
	microbench_start(&mbench_state, name); \
	if (mbench_state.rt.started) \
		microbench_gettime(&mbench_state.rt.t1); \
	for (long microbench_i = 0; microbench_i < mbench_state.rt.repeat; \
			++microbench_i) { \
		code_block; \
	} \
	if (mbench_state.rt.started) { \
		microbench_gettime(&mbench_state.rt.t2); \
		microbench_stop(&mbench_state); \
	} \
}

#define MBENCH_STARTED (mbench_state.rt.started)

#define MBENCH_INC(val) \
	val += mbench_state.params.inc

#define MBENCH_PROBE_STRSTR(name) \
	microbench_probe_strstr(&mbench_state.params, name)

#define MBENCH_GET_REPEAT() microbench_get_repeat(&mbench_state.params)

/* hold all micro-benchmark looping till MBENCH_RELEASE */
#define MBENCH_HOLD() microbench_hold()

/* release micro-benchmark looping after MBENCH_HOLD */
#define MBENCH_RELEASE() microbench_release()

#else

#define MBENCH_INIT() {}
#define MBENCH_FINI() {}

#define MBENCH(arg1, code_block) \
	code_block;

#define MBENCH_STARTED (0)
#define MBENCH_INC(arg) {}

#define MBENCH_PROBE_STRSTR(arg) (0)
#define MBENCH_GET_REPEAT() (0)

#define MBENCH_HOLD() {}
#define MBENCH_RELEASE() {}

#endif

#endif /* MBENCH_H */
