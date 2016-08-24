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

/*
 * search.hpp
 */

#ifndef EXAMPLES_PCHESS_SEARCH_H
#define EXAMPLES_PCHESS_SEARCH_H

#include "chess.hpp"
#include "position.hpp"

#include <chrono>
#include <functional>

#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/transaction.hpp>

namespace examples
{
namespace pchess
{

unsigned long perft(const position *, unsigned depth);

class search {
public:
	void reset(const position &, int depth);
	void negamax(unsigned long node_limit);
	bool is_done() const;
	int get_value() const;
	const move_list &get_PV() const;
	unsigned long get_node_count() const;
	int get_depth() const;

private:
	struct node {
		nvml::obj::p<position> pos;
		nvml::obj::p<move_list> moves;
		nvml::obj::p<unsigned> move_index;
		node *parent;
		nvml::obj::persistent_ptr<node> child;
		nvml::obj::p<int> alpha;
		nvml::obj::p<int> beta;
		nvml::obj::p<int> best_value;
		nvml::obj::p<int> depth;
		nvml::obj::p<bool> is_done;
		nvml::obj::p<move_list> PV;

		node(const position &, int depth);
		node(node *parent);
		~node();

		bool is_in_quiescence_search() const;
		void create_next_child();
		void sort_moves();
		void filter_tactical_moves();
		void new_best_move();
		void child_done();
		bool has_any_moves() const;
		bool is_capture(move) const;
	};

	nvml::obj::p<unsigned long> node_count;
	nvml::obj::persistent_ptr<node> stack_root;

	node *current_node;

	void heal_node_stack();
};
}
}

#endif /* EXAMPLES_PCHESS_SEARCH_H */
