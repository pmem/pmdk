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
 * search.cpp - implementation of the game tree search.
 * The negamax search is designed as a recursive algorithm, storing the
 * nodes of the gametree being visited on the stack. But the thread's
 * stack is volatile memory, so one can't just implement it verbatim,
 * and suppliment the volatile stack with permanent memory. This implementation
 * uses a stack of nodes in pmem, and can stop-and-continue after any node.
 *
 */

#include "search.hpp"
#include "eval.hpp"
#include "position.hpp"

#include <cassert>
#include <cstring>

#include <libpmemobj++/make_persistent.hpp>

using pmem::obj::persistent_ptr;
using pmem::obj::make_persistent;
using pmem::obj::delete_persistent;

namespace examples
{
namespace pchess
{

/*
 * perft(pos, depth)
 * https://chessprogramming.wikispaces.com/Perft
 * For verifying the move generator. This is not really a searching
 * routine, but it demonstrates what a simple recursive backtracking
 * on the gametree would look like.
 */
unsigned long
perft(const position *pos, unsigned depth)
{
	if (depth == 0)
		return 1;

	if (depth == 1)
		return pos->get_moves().count;

	unsigned long count = 0;

	for (move m : pos->get_moves()) {
		position child_node = pos->make_move(m);
		count += perft(&child_node, depth - 1);
	}

	return count;
}

/*
 * The implementation of search::node
 */

/*
 * node's constructor, used for constructing the root node.
 */
search::node::node(const position &p, int depth)
    : pos(p),
      moves(p.get_moves()),
      move_index(0),
      parent(nullptr),
      child(nullptr),
      alpha(-infinite),
      beta(infinite),
      best_value(-infinite),
      depth(depth),
      is_done(false)
{
	sort_moves();
}

/*
 * node's constructor, used for constructing a new child node
 * based an a parent node
 */
search::node::node(node *parent)
    : pos(parent->pos.get_ro().make_move(
	      parent->moves.get_ro()[parent->move_index])),
      moves(pos.get_ro().get_moves()),
      move_index(0),
      parent(parent),
      child(nullptr),
      alpha(-parent->beta),
      beta(-parent->alpha),
      best_value(-infinite),
      depth(parent->depth - 1),
      is_done(false)
{
	if (pos.get_ro().is_checkmate()) {
		best_value = -infinite;
		is_done = true;
		return;
	} else if (pos.get_ro().is_stalemate()) {
		best_value = 0;
		is_done = true;
		return;
	} else if (is_in_quiescence_search()) {
		best_value = eval(&(pos.get_ro())); // stand-pat score
		if (best_value >= beta) {
			is_done = true;
			return;
		}
		filter_tactical_moves();
		if (not has_any_moves()) {
			/*
			 * No tactical moves to try, this is a leaf node.
			 * Just return the result of the static evaluation
			 * to the parent node.
			 */
			is_done = true;
			return;
		}
	}

	sort_moves();
}

search::node::~node()
{
	if (child != nullptr)
		delete_persistent<node>(child);
}

bool
search::node::has_any_moves() const
{
	return moves.get_ro().count != 0;
}

bool
search::node::is_in_quiescence_search() const
{
	// http://chessprogramming.wikispaces.com/Quiescence+Search
	return depth <= 0 and not pos.get_ro().is_in_check();
}

bool
search::node::is_capture(move m) const
{
	return not pos.get_ro().get_board()[m.to].is_empty;
}

void
search::node::filter_tactical_moves()
{
	move_list tacticals;

	for (move m : moves.get_ro()) {
		/*
		 * For now only captures (non empty target square)
		 * and promotions to queen are considered tactical moves.
		 * At least these obvious moves need to be
		 * searched during quiescence search, to avoid a very bad
		 * horizon effect.
		 */
		if (is_capture(m) or m.type == move_type::PROMOTE_QUEEN)
			tacticals.push_back(m);
	}
	moves = tacticals;
}

/*
 * node::sort_moves() - Overwrite the move list with an ordered version.
 * Moves deemed more likely to cause a cutoff should be moved forward in
 * the list.
 */
void
search::node::sort_moves()
{
	move_list ordered;
	const chess_board &board = pos.get_ro().get_board();

	for (move m : moves.get_ro()) {
		if ((is_capture(m) and board[m.to].piece_type != PAWN) or
		    m.type == move_type::PROMOTE_QUEEN)
			ordered.push_back(m);
	}

	for (move m : moves.get_ro()) {
		if (is_capture(m) and board[m.to].piece_type == PAWN)
			ordered.push_back(m);
	}

	for (move m : moves.get_ro()) {
		if (not is_capture(m) and m.type != move_type::PROMOTE_QUEEN)
			ordered.push_back(m);
	}

	moves = ordered;
}

void
search::node::create_next_child()
{
	if (child != nullptr)
		delete_persistent<node>(child);

	child = make_persistent<node>(this);
}

/*
 * node::new_best_move() - A move resulted in a subtree with a better
 * score than any other moves before, so this must be remembered as
 * the "best line" known by the engine. Note: One can not just write
 * some items in the PV member variable, as it is a p<move_list>,
 * and this is expected to be run in a pmem transaction. Using the
 * p<> template the only option for changing the PV, is to overwrite
 * the whole PV. The assignment operator takes care of correctly adding
 * this change to the current transaction. Thus, the use of temporary
 * variable called new_PV.
 */
void
search::node::new_best_move()
{
	assert(child != nullptr);

	move_list new_PV;
	new_PV[0] = moves.get_ro()[move_index];
	new_PV.count = child->PV.get_ro().count + 1;
	std::memcpy(&new_PV.items[1], &child->PV.get_ro().items[0],
		    child->PV.get_ro().count * sizeof(move));
	PV = new_PV;
}

/*
 * node::child_done() - The search of a subtree stemming from a child node
 * is done. The result value must be examined, and incorporated into the
 * value of this node. The node is done, and returns a value to its parent
 * node if there are no more moves to examine, or a beta cut-off happened.
 * https://chessprogramming.wikispaces.com/Alpha-Beta
 */
void
search::node::child_done()
{
	assert(child != nullptr);

	int value = -child->best_value;

	if (value > mate_value)
		--value;

	if (value > best_value) {
		best_value = value;
		if (value > alpha) {
			alpha = value;
			new_best_move();
			if (value >= beta)
				is_done = true; // beta cut-off
		}
	}

	delete_persistent<node>(child);
	child = nullptr;
	move_index = move_index + 1;

	if (move_index >= moves.get_ro().size())
		is_done = true; // no more moves, done here
}

/*
 * The end of the search::node implementation
 */

unsigned long
search::get_node_count() const
{
	return node_count;
}

/*
 * heal_node_stack() - some volatile pointers are used temporarily
 * during search, this method recreates them. As usual here, it is
 * not to be confused with 'volatile' from ISO C, these pointers
 * just point to volatile memory. Each negamax search should start
 * with the current_node pointer pointing to the next node to be
 * examined - the one deepest in the stack currently. Also, each
 * node stores a volatile pointer to its parent node, instead of
 * a persistent_ptr. These addresses can also change between closing
 * and reopening the pmem pool.
 */
void
search::heal_node_stack()
{
	current_node = stack_root.get();

	while (current_node->child != nullptr) {
		current_node->child->parent = current_node;
		current_node = current_node->child.get();
	}
}

/*
 * negamax(node_limit) - perform a negamax search, not visiting more
 * than node_limit nodes in the game tree.
 * This method must be able to continue exactly where it stopped,
 * even if addresses changed during restarting the pchess process.
 */
void
search::negamax(unsigned long node_limit)
{
	if (stack_root == nullptr or is_done())
		return;

	heal_node_stack();

	node_limit += node_count;

	while (node_count < node_limit) {
		while (current_node->is_done) {
			if (current_node->parent != nullptr) {
				current_node = current_node->parent;
				current_node->child_done();
			} else {
				return; // at root, search is finished
			}
		}
		current_node->create_next_child();
		current_node = current_node->child.get();
		node_count = node_count + 1;
	}
}

/*
 * reset(new_root, depth) - Throw away the current state of the search,
 * and prepare for a new search, originating from a new root node.
 */
void
search::reset(const position &new_root, int depth)
{
	using namespace std::chrono_literals;

	if (stack_root != nullptr)
		delete_persistent<node>(stack_root);

	stack_root = make_persistent<node>(new_root, depth);
	node_count = 0;
}

/*
 * is_done() - Let the user of the search class know when the search
 * is done. This is basically when all descendants of the root node have
 * been visited, thus equivalent to stack_root->done.
 */
bool
search::is_done() const
{
	if (stack_root != nullptr)
		return stack_root->is_done;
	else
		return false;
}

/*
 * get_value() - return the evaluation of the root node, which of course
 * is only available, when the whole search is done.
 */
int
search::get_value() const
{
	assert(stack_root != nullptr);
	assert(stack_root->is_done);

	return stack_root->best_value;
}

/*
 * get_PV() - return the PV that was finally collected at the root node,
 * this is the line of best moves expected by the computer player. This
 * is of course only available, when the whole search is done.
 */
const move_list &
search::get_PV() const
{
	assert(stack_root != nullptr);
	assert(stack_root->is_done);

	return stack_root->PV.get_ro();
}

int
search::get_depth() const
{
	assert(stack_root != nullptr);

	return stack_root->depth;
}
}
}
