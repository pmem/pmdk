// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2020, Intel Corporation */

#include "future.h"

void *
future_context_get_data(struct future_context *context)
{
	return (char *)context + sizeof(struct future_context);
}

void *
future_context_get_output(struct future_context *context)
{
	return future_context_get_data(context) + context->data_size;
}

size_t
future_context_get_size(struct future_context *context)
{
	return context->data_size + context->output_size;
}

enum future_state
future_poll(struct future *fut, struct future_waker waker)
{
	return (fut->context.state = fut->task(&fut->context, waker));
}

enum future_state
async_chain_impl(struct future_context *ctx, struct future_waker waker)
{
	uint8_t *data = future_context_get_data(ctx);

	struct future_chain_entry *entry = (struct future_chain_entry *)(data);
	size_t used_data = 0;

	while (entry != NULL) {
		used_data += sizeof(struct future_chain_entry) +
			future_context_get_size(&entry->future.context);
		struct future_chain_entry *next = used_data != ctx->data_size
			? (struct future_chain_entry *)(data + used_data)
			: NULL;
		if (entry->future.context.state != FUTURE_STATE_COMPLETE) {
			future_poll(&entry->future, waker);
			if (entry->future.context.state ==
				    FUTURE_STATE_COMPLETE &&
			    entry->map) {
				entry->map(&entry->future.context,
					   next ? &next->future.context : ctx,
					   entry->arg);
			} else {
				return FUTURE_STATE_RUNNING;
			}
		}
		entry = next;
	}

	return FUTURE_STATE_COMPLETE;
}

static void
future_wake_noop(void *data)
{

}

struct future_waker
future_noop_waker(void)
{
	return (struct future_waker){NULL, future_wake_noop};
}
