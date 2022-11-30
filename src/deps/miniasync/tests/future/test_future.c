// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2022, Intel Corporation */

#include "libminiasync/future.h"
#include "test_helpers.h"
#include "core/util.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_MAX_COUNT 10
#define FAKE_NOTIFIER ((void *)((uintptr_t)(0xDEADBEEF)))
#define FAKE_MAP_ARG ((void *)((uintptr_t)(0xFEEDCAFE)))

struct countup_data {
	int counter;
	int max_count;
};

struct countup_output {
	int result;
};

FUTURE(countup_fut, struct countup_data, struct countup_output);

enum future_state
countup_task(struct future_context *context,
	struct future_notifier *notifier)
{
	UT_ASSERTeq(notifier, FAKE_NOTIFIER);

	struct countup_data *data = future_context_get_data(context);
	data->counter++;
	if (data->counter == data->max_count) {
		struct countup_output *output =
			future_context_get_output(context);
		output->result += 1;
		return FUTURE_STATE_COMPLETE;
	} else {
		return FUTURE_STATE_RUNNING;
	}
}

struct countup_fut
async_countup(int max_count)
{
	struct countup_fut fut = {.output.result = 0};
	FUTURE_INIT(&fut, countup_task);
	fut.data.counter = 0;
	fut.data.max_count = max_count;
	fut.output.result = 0;

	return fut;
}

void
test_single_future(void)
{
	struct countup_fut up = async_countup(TEST_MAX_COUNT);
	UT_ASSERTeq(FUTURE_STATE(&up), FUTURE_STATE_IDLE);

	struct countup_output *output = FUTURE_OUTPUT(&up);
	UT_ASSERTeq(output->result, 0);

	struct countup_data *data = FUTURE_DATA(&up);
	UT_ASSERTeq(data->counter, 0);

	enum future_state state = FUTURE_STATE_RUNNING;

	for (int i = 0; i < TEST_MAX_COUNT; ++i) {
		UT_ASSERTeq(state, FUTURE_STATE_RUNNING);
		UT_ASSERTeq(FUTURE_STATE(&up), i == 0 ?
			FUTURE_STATE_IDLE : FUTURE_STATE_RUNNING);
		UT_ASSERTeq(data->counter, i);
		UT_ASSERTeq(output->result, 0);

		state = future_poll(FUTURE_AS_RUNNABLE(&up), FAKE_NOTIFIER);
	}
	UT_ASSERTeq(data->counter, TEST_MAX_COUNT);
	UT_ASSERTeq(output->result, 1);
	UT_ASSERTeq(state, FUTURE_STATE_COMPLETE);

	/* polling on a complete future is a noop */
	state = future_poll(FUTURE_AS_RUNNABLE(&up), FAKE_NOTIFIER);

	UT_ASSERTeq(data->counter, TEST_MAX_COUNT);
	UT_ASSERTeq(output->result, 1);
	UT_ASSERTeq(state, FUTURE_STATE_COMPLETE);
}

struct countdown_data {
	int counter;
};

struct countdown_output {
	int result;
};

FUTURE(countdown_fut, struct countdown_data, struct countdown_output);

enum future_state
countdown_task(struct future_context *context,
	struct future_notifier *notifier)
{
	UT_ASSERTeq(notifier, FAKE_NOTIFIER);

	struct countdown_data *data = future_context_get_data(context);
	data->counter--;
	if (data->counter == 0) {
		struct countdown_output *output =
			future_context_get_output(context);
		output->result += 1;
		return FUTURE_STATE_COMPLETE;
	} else {
		return FUTURE_STATE_RUNNING;
	}
}

struct countdown_fut
async_countdown(int count)
{
	struct countdown_fut fut = {.output.result = 0};
	FUTURE_INIT(&fut, countdown_task);
	fut.data.counter = count;
	fut.output.result = 0;

	return fut;
}

struct up_down_data {
	FUTURE_CHAIN_ENTRY(struct countup_fut, up);
	FUTURE_CHAIN_ENTRY(struct countdown_fut, down);
};

struct up_down_output {
	int result_sum;
};

FUTURE(up_down_fut, struct up_down_data, struct up_down_output);

void
up_to_down_map(struct future_context *lhs, struct future_context *rhs,
	void *arg)
{
	UT_ASSERTeq(arg, FAKE_MAP_ARG);

	struct countup_data *up_data = future_context_get_data(lhs);
	struct countup_output *up_output = future_context_get_output(lhs);
	struct countdown_data *down_data = future_context_get_data(rhs);
	struct countdown_output *down_output = future_context_get_output(rhs);

	down_data->counter = up_data->counter;
	down_output->result += up_output->result;
}

void
down_to_result_map(struct future_context *lhs, struct future_context *rhs,
	void *arg)
{
	UT_ASSERTeq(arg, FAKE_MAP_ARG);
	struct countdown_data *down_data = future_context_get_data(lhs);
	UT_ASSERTeq(down_data->counter, 0);

	struct countdown_output *down_output = future_context_get_output(lhs);
	struct up_down_output *output = future_context_get_output(rhs);
	output->result_sum = down_output->result;
}

struct up_down_fut
async_up_down(int count)
{
	struct up_down_fut fut = {.output.result_sum = 0};
	FUTURE_CHAIN_ENTRY_INIT(&fut.data.up, async_countup(count),
		up_to_down_map, FAKE_MAP_ARG);
	FUTURE_CHAIN_ENTRY_INIT(&fut.data.down, async_countdown(0),
		down_to_result_map, FAKE_MAP_ARG);
	FUTURE_CHAIN_INIT(&fut);

	/* regular chain entry init: all entries are initialized right away */
	UT_ASSERTeq(FUTURE_CHAIN_ENTRY_IS_INITIALIZED(&fut.data.up), true);
	UT_ASSERTeq(FUTURE_CHAIN_ENTRY_IS_INITIALIZED(&fut.data.down), true);

	return fut;
}

void
test_chained_future()
{
	struct up_down_fut fut = async_up_down(TEST_MAX_COUNT);
	UT_ASSERTeq(FUTURE_STATE(&fut), FUTURE_STATE_IDLE);

	for (int i = 0; i < TEST_MAX_COUNT * 2; ++i) {
		future_poll(FUTURE_AS_RUNNABLE(&fut), FAKE_NOTIFIER);
	}

	UT_ASSERTeq(FUTURE_STATE(&fut), FUTURE_STATE_COMPLETE);

	struct up_down_output *output = FUTURE_OUTPUT(&fut);
	UT_ASSERTeq(output->result_sum, 2);
}

struct multiply_data {
	int a;
	int b;
};

struct multiply_output {
	int result;
};

FUTURE(multiply_fut, struct multiply_data, struct multiply_output);

struct multiply_fut
async_multiply(int a, int b)
{
	struct multiply_fut fut;
	FUTURE_INIT_COMPLETE(&fut);
	fut.data.a = a;
	fut.data.b = b;
	fut.output.result = a * b;

	return fut;
}

void
test_completed_future()
{
	struct multiply_fut fut = async_multiply(2, 3);
	UT_ASSERTeq(FUTURE_STATE(&fut), FUTURE_STATE_COMPLETE);
	FUTURE_BUSY_POLL(&fut);
	UT_ASSERTeq(FUTURE_STATE(&fut), FUTURE_STATE_COMPLETE);
}

struct multiply_up_down_data {
	FUTURE_CHAIN_ENTRY(struct multiply_fut, mul);
	FUTURE_CHAIN_ENTRY_LAST(struct up_down_fut, up_down);
	int num;
	int count;
};

struct multiply_up_down_output {
	int result_sum;
};

FUTURE(multiply_up_down_fut,
	struct multiply_up_down_data, struct multiply_up_down_output);

void
multiply_init(void *future, struct future_context *chain_fut, void *arg)
{
	struct multiply_up_down_data *data = future_context_get_data(chain_fut);
	struct multiply_fut fut = async_multiply(data->count, data->num);
	memcpy(future, &fut, sizeof(fut));
}

void
up_down_init(void *future, struct future_context *chain_fut, void *arg)
{
	struct multiply_up_down_data *data = future_context_get_data(chain_fut);
	struct up_down_fut fut = async_up_down(data->mul.fut.output.result);
	memcpy(future, &fut, sizeof(fut));
}

void
up_down_to_output(struct future_context *lhs,
	struct future_context *rhs, void *arg)
{
	struct up_down_output *ud_output =
		future_context_get_output(lhs);
	struct multiply_up_down_output *mud_output =
		future_context_get_output(rhs);
	mud_output->result_sum = ud_output->result_sum;
}

struct multiply_up_down_fut
async_multiply_up_down(int count, int num)
{
	struct multiply_up_down_fut fut = {.output.result_sum = 0};
	fut.data.count = count;
	fut.data.num = num;
	FUTURE_CHAIN_ENTRY_LAZY_INIT(&fut.data.mul,
		multiply_init, NULL,
		NULL, NULL);
	FUTURE_CHAIN_ENTRY_LAZY_INIT(&fut.data.up_down,
		up_down_init, NULL,
		up_down_to_output, NULL);
	FUTURE_CHAIN_INIT(&fut);

	return fut;
}

void
test_lazy_init()
{
	struct multiply_up_down_fut fut = async_multiply_up_down(5, 5);
	UT_ASSERTeq(FUTURE_CHAIN_ENTRY_IS_INITIALIZED(&fut.data.mul), false);
	UT_ASSERTeq(FUTURE_CHAIN_ENTRY_IS_INITIALIZED(&fut.data.up_down),
		false);

	while (future_poll(FUTURE_AS_RUNNABLE(&fut), FAKE_NOTIFIER) !=
		FUTURE_STATE_COMPLETE) { WAIT(); }

	/* we can assume, after polling, that all entries are initialized */
	UT_ASSERTeq(FUTURE_CHAIN_ENTRY_IS_INITIALIZED(&fut.data.mul), true);
	UT_ASSERTeq(FUTURE_CHAIN_ENTRY_IS_INITIALIZED(&fut.data.up_down), true);

	struct multiply_up_down_output *mud_output = FUTURE_OUTPUT(&fut);
	UT_ASSERTeq(mud_output->result_sum, 2);
	struct multiply_up_down_data *mud_data = FUTURE_DATA(&fut);
	UT_ASSERTeq(mud_data->up_down.fut.data.up.fut.data.counter, 5 * 5);
}

int
main(void)
{
	test_single_future();
	test_chained_future();
	test_completed_future();
	test_lazy_init();

	return 0;
}
