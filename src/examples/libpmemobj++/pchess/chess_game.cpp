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
 * chess_game.cpp - implementation of the chess_game class
 */

#include "chess_game.hpp"

#include <libpmemobj++/make_persistent.hpp>

using std::string;
using std::chrono::milliseconds;
using pmem::obj::make_persistent;
using pmem::obj::persistent_ptr;
using namespace std::chrono_literals;

namespace examples
{
namespace pchess
{

chess_game::history_item::history_item(string FEN)
    : previous(nullptr), pos(position(FEN)), next(nullptr)
{
}

chess_game::history_item::history_item(
	pmem::obj::persistent_ptr<history_item> parent, move m)
    : previous(parent), pos(parent->pos.get_ro().make_move(m)), next(nullptr)
{
}

chess_game::chess_game()
    : head(make_persistent<history_item>(starting_FEN)),
      tail(head),
      is_game_in_progress(false)
{
}

const position *
chess_game::current_position() const
{
	return &tail->pos.get_ro();
}

void
chess_game::undo_move()
{
	if (tail->previous == nullptr)
		return;

	auto temp = tail;

	tail = tail->previous;
	tail->next = nullptr;
	pmem::obj::delete_persistent<history_item>(temp);
}

void
chess_game::reset(std::string FEN)
{
	position new_head(FEN);

	while (tail->previous != nullptr)
		undo_move();

	head->pos = new_head;
}

side
chess_game::next_to_move() const
{
	return current_position()->get_side_to_move();
}

void
chess_game::start()
{
	is_game_in_progress = true;
}

void
chess_game::stop()
{
	is_game_in_progress = false;
}

bool
chess_game::is_in_progress() const
{
	return is_game_in_progress;
}

void
chess_game::make_move(move m)
{
	tail->next = make_persistent<history_item>(tail, m);
	tail->next_move = m;
	tail = tail->next;

	if (is_game_over())
		stop();
}

bool
chess_game::is_game_over() const
{
	return current_position()->is_checkmate() or
		current_position()->is_stalemate();
}
}
}
