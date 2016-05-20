/*
 * Copyright 2016, Intel Corporation
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

#include "ctree_map_transient.hpp"
#include <ctree_map_persistent.hpp>
#include <iostream>
#include <libpmemobj/pool.hpp>
#include <memory>
#include <unistd.h>

namespace
{

using nvml::obj::persistent_ptr;
using nvml::obj::make_persistent;
using nvml::obj::transaction;
using nvml::obj::delete_persistent;
using nvml::obj::pool;
using nvml::obj::pool_base;

/* convenience typedefs */
typedef long long int value_t;
typedef uint64_t key_type;
typedef examples::ctree_map_p<key_type, value_t> pmap;
typedef examples::ctree_map_transient<key_type, value_t> vmap;

const std::string LAYOUT = "";

/* available map operations */
enum queue_op {
	UNKNOWN_QUEUE_OP,
	MAP_INSERT,
	MAP_INSERT_NEW,
	MAP_GET,
	MAP_REMOVE,
	MAP_REMOVE_FREE,
	MAP_CLEAR,
	MAP_PRINT,

	MAX_QUEUE_OP
};

/* queue operations strings */
const char *ops_str[MAX_QUEUE_OP] = {"",      "insert", "insert_new",
				     "get",   "remove", "remove_free",
				     "clear", "print"};

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

struct root {
	persistent_ptr<pmap> ptree;
};

/*
 * printer -- (internal) print the value for the given key
 */
template <typename T>
int
printer(key_type key, T value, void *)
{
	std::cout << "map[" << key << "] = " << *value << std::endl;
	return 0;
}

/*
 * insert -- (internal) insert value into the map
 */
template <typename T>
void
insert(pool_base pop, T &map, char *argv[], int &argn)
{
	map->insert(atoll(argv[argn]), new value_t(atoll(argv[argn + 1])));
	argn += 2;
}

/*
 * remove -- (internal) remove value from map
 */
template <typename T>
void
remove(pool_base pop, T &map, char *argv[], int &argn)
{
	auto val = map->remove(atoll(argv[argn++]));
	if (val) {
		std::cout << *val << std::endl;
		delete val;
	} else {
		std::cout << "Entry not found\n";
	}
}

/*
 * remove -- (internal) remove specialization for persistent ctree
 */
template <>
void
remove<persistent_ptr<pmap>>(pool_base pop, persistent_ptr<pmap> &map,
			     char *argv[], int &argn)
{
	auto val = map->remove(atoll(argv[argn++]));
	if (val) {
		std::cout << *val << std::endl;
		transaction::exec_tx(pop,
				     [&] { delete_persistent<value_t>(val); });
	} else {
		std::cout << "Entry not found\n";
	}
}

/*
 * insert -- (internal) insert specialization for persistent ctree
 */
template <>
void
insert<persistent_ptr<pmap>>(pool_base pop, persistent_ptr<pmap> &map,
			     char *argv[], int &argn)
{
	transaction::exec_tx(pop, [&] {
		map->insert(atoll(argv[argn]),
			    make_persistent<value_t>(atoll(argv[argn + 1])));
	});
	argn += 2;
}

/*
 * exec_op -- (internal) execute single operation
 */
template <typename K, typename T>
void
exec_op(pool_base pop, T &map, queue_op op, char *argv[], int &argn)
{
	switch (op) {
		case MAP_INSERT_NEW:
			map->insert_new(atoll(argv[argn]),
					atoll(argv[argn + 1]));
			argn += 2;
			break;
		case MAP_INSERT:
			insert(pop, map, argv, argn);
			break;
		case MAP_GET: {
			auto val = map->get(atoll(argv[argn]));
			if (val)
				std::cout << *val << std::endl;
			else
				std::cout << "key not found\n";
			break;
		}
		case MAP_REMOVE:
			remove(pop, map, argv, argn);
			break;
		case MAP_REMOVE_FREE:
			map->remove_free(atoll(argv[4]));
			break;
		case MAP_CLEAR:
			map->clear();
			break;
		case MAP_PRINT:
			map->foreach (printer<typename K::value_type>, 0);
			break;
		default:
			throw std::invalid_argument("invalid queue operation");
			break;
	}
}
}

int
main(int argc, char *argv[])
{
	if (argc < 4) {
		std::cerr << "usage: " << argv[0]
			  << " file-name <persistent|volatile> "
			     "[insert|insert_new "
			     "<key value>|get <key>|remove <key> | remove_free "
			     "<key>]"
			  << std::endl;
		return 1;
	}

	std::string path = argv[1];
	std::string type = argv[2];

	pool<root> pop;

	if (access(path.c_str(), F_OK) != 0) {
		pop = pool<root>::create(path, LAYOUT, PMEMOBJ_MIN_POOL,
					 S_IRWXU);
	} else {
		pop = pool<root>::open(path, LAYOUT);
	}

	auto q = pop.get_root();
	if (!q->ptree) {
		transaction::exec_tx(
			pop, [&] { q->ptree = make_persistent<pmap>(); });
	}

	auto vtree = std::make_shared<vmap>();

	for (int i = 3; i < argc;) {
		queue_op op = parse_queue_op(argv[i++]);
		if (type == "volatile")
			exec_op<vmap>(pop, vtree, op, argv, i);
		else
			exec_op<pmap>(pop, q->ptree, op, argv, i);
	}

	pop.close();

	return 0;
}
