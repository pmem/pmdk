/*
 * Copyright (c) 2015-2016, Intel Corporation
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
 *     * Neither the name of Intel Corporation nor the names of its
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
 * queue.cpp -- queue example implemented using pmemobj cpp bindings
 *
 * Please see pmem.io blog posts for more details.
 */

#include <libpmemobj/persistent_ptr.hpp>
#include <libpmemobj/p.hpp>
#include <unistd.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <stdexcept>
#include <iostream>

#define	LAYOUT "queue"

namespace {

/* available queue operations */
enum queue_op {
	UNKNOWN_QUEUE_OP,
	QUEUE_PUSH,
	QUEUE_POP,
	QUEUE_SHOW,

	MAX_QUEUE_OP
};

/* queue operations strings */
const char *ops_str[MAX_QUEUE_OP] = {"", "push", "pop", "show"};

/*
 * parse_queue_op -- parses the operation string and returns matching queue_op
 */
queue_op
parse_queue_op(const char *str)
{
	for (int i = 0; i < MAX_QUEUE_OP; ++i)
		if (strcmp(str, ops_str[i]) == 0)
			return (queue_op)i;

	return UNKNOWN_QUEUE_OP;
}

}

using namespace nvml::obj;

/*
 * Persistent memory list-based queue
 *
 * A simple, not template based, implementation of queue using
 * libpmemobj C++ API. It demonstrates the basic features of persistent_ptr<>
 * and p<> classes.
 */
class pmem_queue {

	/* entry in the list */
	struct pmem_entry {
		persistent_ptr<pmem_entry> next;
		p<uint64_t> value;
	};

public:
	/*
	 * Inserts a new element at the end of the queue.
	 */
	void
	push(PMEMobjpool *pop, uint64_t value)
	{
		TX_BEGIN(pop) {
			persistent_ptr<pmem_entry> n =
				pmemobj_tx_alloc(sizeof (pmem_entry), 0);
			n->value = value;
			n->next = nullptr;

			if (head == nullptr && tail == nullptr) {
				head = tail = n;
			} else {
				tail->next = n;
				tail = n;
			}
		} TX_ONABORT {
			throw std::runtime_error("transaction aborted");
		} TX_END
	}

	/*
	 * Removes the first element in the queue.
	 */
	uint64_t
	pop(PMEMobjpool *pop)
	{
		uint64_t ret = 0;
		TX_BEGIN(pop) {
			if (head == nullptr)
				throw std::logic_error("empty queue");

			ret = head->value;
			auto n = head->next;

			pmemobj_tx_free(head.raw());
			head = n;

			if (head == nullptr)
				tail = nullptr;
		} TX_ONABORT {
			throw std::runtime_error("transaction aborted");
		} TX_END

		return ret;
	}

	/*
	 * Prints the entire contents of the queue.
	 */
	void
	show(void)
	{
		for (auto n = head; n != nullptr; n = n->next)
			std::cout << n->value << std::endl;
	}
private:
	persistent_ptr<pmem_entry> head;
	persistent_ptr<pmem_entry> tail;
};

int
main(int argc, char *argv[])
{
	if (argc < 3) {
		std::cerr <<
			"usage: " << argv[0] <<
			" file-name [push [value]|pop|show]" << std::endl;
		return 1;
	}

	const char *path = argv[1];

	queue_op op = parse_queue_op(argv[2]);

	PMEMobjpool *pop = NULL;

	if (access(path, F_OK) != 0) {
		if ((pop = pmemobj_create(path, LAYOUT,
			PMEMOBJ_MIN_POOL, S_IRWXU)) == NULL) {
			std::cerr << "failed to create pool" << std::endl;
			return 1;
		}
	} else {
		if ((pop = pmemobj_open(path, LAYOUT)) == NULL) {
			std::cerr << "failed to open pool" << std::endl;
			return 1;
		}
	}

	persistent_ptr<pmem_queue> q = pmemobj_root(pop, sizeof (pmem_queue));
	switch (op) {
		case QUEUE_PUSH:
			q->push(pop, atoll(argv[3]));
		break;
		case QUEUE_POP:
			std::cout << q->pop(pop) << std::endl;
		break;
		case QUEUE_SHOW:
			q->show();
		break;
		default:
			throw std::invalid_argument("invalid queue operation");
		break;
	}

	pmemobj_close(pop);
}
