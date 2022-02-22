// SPDX-License-Identifier: BSD-3-Clause
/* Copyright 2019-2022, Intel Corporation */

#include "libminiasync/future.h"
#include "core/util.h"

void *
future_context_get_data(struct future_context *context)
{
	return (char *)context + sizeof(struct future_context);
}

void *
future_context_get_output(struct future_context *context)
{
	return (char *)future_context_get_data(context) + context->data_size;
}

size_t
future_context_get_size(struct future_context *context)
{
	return context->data_size + context->output_size;
}

enum future_state
future_poll(struct future *fut, struct future_notifier *notifier)
{
	return (fut->context.state = fut->task(&fut->context, notifier));
}

enum future_state
async_chain_impl(struct future_context *ctx, struct future_notifier *notifier)
{
	uint8_t *data = future_context_get_data(ctx);

	struct future_chain_entry *entry = (struct future_chain_entry *)(data);
	size_t used_data = 0;

	/*
	 * This will iterate to the first non-complete future in the chain
	 * and then call poll it once.
	 * Futures must be laid out sequentially in memory for this to work.
	 */
	while (entry != NULL) {
		used_data += sizeof(struct future_chain_entry) +
			future_context_get_size(&entry->future.context);
		struct future_chain_entry *next = used_data != ctx->data_size
			? (struct future_chain_entry *)(data + used_data)
			: NULL;
		if (entry->future.context.state != FUTURE_STATE_COMPLETE) {
			future_poll(&entry->future, notifier);
			if (entry->future.context.state ==
				    FUTURE_STATE_COMPLETE &&
			    entry->map) {
				entry->map(&entry->future.context,
							next ?
							&next->future.context
							: ctx,
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
	/* to avoid unused formal parameter warning */
	SUPPRESS_UNUSED(data);
}

struct future_notifier
future_noop_notifier(void)
{
	struct future_notifier notifier = {0};
	notifier.poller.ptr_to_monitor = NULL;
	notifier.waker.wake = future_wake_noop;
	notifier.waker.data = NULL;

	return notifier;
}
