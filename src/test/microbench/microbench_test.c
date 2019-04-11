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
 * microbench_test.c -- unit test for microbench.h
 */

#include "unittest.h"

#include "microbench.h"

#ifdef MBENCH_ENABLED

#define WORKLOAD_PROBE "workload"
#define OTHER_PROBE "other"

#define STRSTR_STRING "lo" /* substring of WORKLOAD_PROBE */

/*
 * mbench_init -- read micro-benchmarking state from environment
 *
 * additionally close log file if needed
 */
static void
mbench_init(struct microbench_state_t *state)
{
	memset(state, 0, sizeof(*state));

	microbench_init(state);

	if (state->params.log_name)
		fclose(state->log);

	state->log = NULL;
}

#endif /* MBENCH_ENABLED */

/*
 * workload -- run op inside micro-benchmarking section
 *
 * without micro-benchmarking should increase counter by 1
 * with micro-benchmarking should increase counter by MBENCH_REPEAT
 */
static void
workload(int *counter)
{
	MBENCH(WORKLOAD_PROBE, {
		*counter += 1;
	});
}

/*
 * workload_other -- other workload inside micro-benchmarking section
 *
 * without micro-benchmarking should increase counter by 2
 * with micro-benchmarking should increase counter by MBENCH_REPEAT * 2
 */
static void
workload_other(int *counter)
{
	MBENCH(OTHER_PROBE, {
		*counter += 2;
	});
}

/*
 * prep -- warm up workload; inside the MBENCH_HOLD/_RELEASE section
 *
 * Should increase counter by 3
 */
static void
prep(int *counter)
{
	/* single hold */
	MBENCH_HOLD();
	workload(counter);
	MBENCH_RELEASE();

	/* double hold to test the hold nesting */
	MBENCH_HOLD();
	MBENCH_HOLD();

	workload(counter);

	MBENCH_RELEASE();
	workload(counter);

	MBENCH_RELEASE();

#ifdef MBENCH_ENABLED
	UT_ASSERTeq(getenv(MICROBENCH_HOLD_ENV), NULL);
#endif
}

/*
 * test_hold_release_0 -- preparation outside the MBENCH_INIT/_FINI section
 */
static int
test_hold_release_0(const struct test_case *tc, int argc, char *argv[])
{
	int counter = 0;

	prep(&counter);

#ifdef MBENCH_ENABLED
	/* does not make sense */
	UT_ASSERT(0); /* should not reach here */
#else
	/* does not take effect */
	UT_ASSERTeq(counter, 3);

	return 0;
#endif
}

/*
 * test_hold_release_1 -- preparation inside the MBENCH_INIT/_FINI section
 */
static int
test_hold_release_1(const struct test_case *tc, int argc, char *argv[])
{
	int counter = 0;

	MBENCH_INIT();
	prep(&counter);
	MBENCH_FINI();

#ifdef MBENCH_ENABLED
	struct microbench_state_t state;
	mbench_init(&state);
	if (state.params.probe)
		UT_ASSERTeq(strcmp(state.params.probe, WORKLOAD_PROBE), 0);
#endif

	/* but does not take effect anyway */
	UT_ASSERTeq(counter, 3);

	return 0;
}

/*
 * test_start_stop_0 -- probe outside the MBENCH_INIT/_FINI section
 */
static int
test_start_stop_0(const struct test_case *tc, int argc, char *argv[])
{
	int counter = 0;
	/* MBENCH_START/_STOP outside MBENCH_INIT/_FINI is invalid */
	workload(&counter);

#ifdef MBENCH_ENABLED
	UT_ASSERT(0); /* should not reach here */
#else
	return 0;
#endif
}

/*
 * test_start_stop_1 -- probe inside the MBENCH_INIT/_FINI section
 */
static int
test_start_stop_1(const struct test_case *tc, int argc, char *argv[])
{
	int counter = 0;

	MBENCH_INIT();
	workload(&counter);
	MBENCH_FINI();

#ifdef MBENCH_ENABLED
	/* assume WORKLOAD_PROBE is enabled */
	struct microbench_state_t state;
	mbench_init(&state);
	if (state.params.probe)
		UT_ASSERTeq(strcmp(state.params.probe, WORKLOAD_PROBE), 0);

	UT_ASSERTeq(counter, state.params.repeat);
#else
	UT_ASSERTeq(counter, 1);
#endif

	return 0;
}

/*
 * test_start_stop_2 -- other probe inside the MBENCH_INIT/_FINI section
 */
static int
test_start_stop_2(const struct test_case *tc, int argc, char *argv[])
{
	int counter = 0;

	MBENCH_INIT();
	workload_other(&counter);
	MBENCH_FINI();

#ifdef MBENCH_ENABLED
	/* assume enabled probe != OTHER_PROBE */
	struct microbench_state_t state;
	mbench_init(&state);
	if (state.params.probe)
		UT_ASSERTne(strcmp(state.params.probe, OTHER_PROBE), 0);
#endif

	UT_ASSERTeq(counter, 2);

	return 0;
}

/*
 * test_start_stop_3 -- probe inside double the MBENCH_INIT/_FINI section
 */
static int
test_start_stop_3(const struct test_case *tc, int argc, char *argv[])
{
	/* double MBENCH_INIT is invalid */
	MBENCH_INIT();
	MBENCH_INIT();

#ifdef MBENCH_ENABLED
	UT_ASSERT(0); /* should not reach here */
#else
	return 0;
#endif
}

#ifdef MBENCH_ENABLED

/*
 * MBENCH_IF, MBENCH_INC and MBENCH_PROBE_STRSTR won't compile if MBENCH_ENABLED
 * is not defined
 */

/*
 * test_if -- MBENCH_IF inside the MBENCH_INIT/_FINI section
 *
 * Outside the MBENCH_INIT/_FINI section MBENCH_INC should fail to compile
 */
static int
test_if(const struct test_case *tc, int argc, char *argv[])
{
	int counter = 0;

	MBENCH_INIT();
	MBENCH(WORKLOAD_PROBE, {
		if (MBENCH_STARTED) {
			++counter;
		}
	});
	MBENCH_FINI();

	/* assume TEST_IF_PROBE is enabled */
	struct microbench_state_t state;
	mbench_init(&state);
	if (state.params.probe) {
		UT_ASSERTeq(strcmp(state.params.probe, WORKLOAD_PROBE), 0);
		UT_ASSERTeq(counter, state.params.repeat);
	} else {
		UT_ASSERTeq(counter, 0);
	}

	return 0;
}

/*
 * test_inc_0 -- MBENCH_INC without the MBENCH_INIT/_FINI section
 */
static int
test_inc_0(const struct test_case *tc, int argc, char *argv[])
{
	int counter = 0;

	/* should not increase */
	MBENCH_INC(counter);

	UT_ASSERTeq(counter, 0);

	return 0;
}

/*
 * test_inc_1 -- MBENCH_INC inside the MBENCH_INIT/_FINI section
 */
static int
test_inc_1(const struct test_case *tc, int argc, char *argv[])
{
	int counter = 0;

	MBENCH_INIT();
	MBENCH_INC(counter);
	MBENCH_FINI();

	/* should increase by MBENCH_INC */
	struct microbench_state_t state;
	mbench_init(&state);
	UT_ASSERT(state.params.inc > 0);
	UT_ASSERTeq(counter, state.params.inc);

	return 0;
}

/*
 * test_inc_2 -- MBENCH_INC after the MBENCH_INIT/_FINI section
 */
static int
test_inc_2(const struct test_case *tc, int argc, char *argv[])
{
	int counter = 0;

	MBENCH_INIT();
	MBENCH_FINI();

	/* should not increase */
	MBENCH_INC(counter);

	UT_ASSERTeq(counter, 0);

	return 0;
}

/*
 * test_strstr_0 -- MBENCH_PROBE_STRSTR without the MBENCH_INIT/_FINI section
 */
static int
test_strstr_0(const struct test_case *tc, int argc, char *argv[])
{
	/* assumes WORKLOAD_PROBE is enabled */
	struct microbench_state_t state;
	mbench_init(&state);
	if (state.params.probe)
		UT_ASSERTeq(strcmp(state.params.probe, WORKLOAD_PROBE), 0);

	/*
	 * MBENCH_PROBE_STRSTR outside the MBENCH_INIT/_FINI section does not
	 * make sense
	 */
	UT_ASSERT(!MBENCH_PROBE_STRSTR(STRSTR_STRING));

	return 0;
}

/*
 * test_strstr_1 -- MBENCH_PROBE_STRSTR inside the MBENCH_INIT/_FINI section
 */
static int
test_strstr_1(const struct test_case *tc, int argc, char *argv[])
{
	/* assumes WORKLOAD_PROBE is enabled */
	struct microbench_state_t state;
	mbench_init(&state);
	if (state.params.probe)
		UT_ASSERTeq(strcmp(state.params.probe, WORKLOAD_PROBE), 0);

	MBENCH_INIT();

	if (state.params.probe) {
		/* should find a match */
		UT_ASSERT(MBENCH_PROBE_STRSTR(STRSTR_STRING));
	} else {
		UT_ASSERT(!MBENCH_PROBE_STRSTR(STRSTR_STRING));
	}

	MBENCH_FINI();

	return 0;
}

/*
 * test_strstr_2 -- MBENCH_PROBE_STRSTR after the MBENCH_INIT/_FINI section
 */
static int
test_strstr_2(const struct test_case *tc, int argc, char *argv[])
{
	MBENCH_INIT();
	MBENCH_FINI();

	/* assumes WORKLOAD_PROBE is enabled */
	struct microbench_state_t state;
	mbench_init(&state);
	if (state.params.probe)
		UT_ASSERTeq(strcmp(state.params.probe, WORKLOAD_PROBE), 0);

	/*
	 * MBENCH_PROBE_STRSTR outside the MBENCH_INIT/_FINI section does not
	 * make sense
	 */
	UT_ASSERT(!MBENCH_PROBE_STRSTR(STRSTR_STRING));

	return 0;
}

#endif /* MBENCH_ENABLED */

/*
 * test_cases -- available test cases
 */
static struct test_case test_cases[] = {
	TEST_CASE(test_hold_release_0),
	TEST_CASE(test_hold_release_1),
	TEST_CASE(test_start_stop_0),
	TEST_CASE(test_start_stop_1),
	TEST_CASE(test_start_stop_2),
	TEST_CASE(test_start_stop_3),

#ifdef MBENCH_ENABLED
	TEST_CASE(test_if),
	TEST_CASE(test_inc_0),
	TEST_CASE(test_inc_1),
	TEST_CASE(test_inc_2),
	TEST_CASE(test_strstr_0),
	TEST_CASE(test_strstr_1),
	TEST_CASE(test_strstr_2),
#endif
};

#define NTESTS	(sizeof(test_cases) / sizeof(test_cases[0]))

int
main(int argc, char *argv[])
{
	START(argc, argv, "microbench");

	TEST_CASE_PROCESS(argc, argv, test_cases, NTESTS);

	DONE(NULL);
}
