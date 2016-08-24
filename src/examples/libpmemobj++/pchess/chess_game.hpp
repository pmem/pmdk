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
 * chess_game.hpp - A class to store the history of a chess game,
 * basically a queue of chess positions. Also, a simple boolean flag
 * to indicate a game being in progress (this would really matter more
 * if game time would be measured).
 */

#ifndef EXAMPLES_PCHESS_CHESS_GAME_H
#define EXAMPLES_PCHESS_CHESS_GAME_H

#include <chrono>
#include <functional>

#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>

#include "position.hpp"

namespace examples
{
namespace pchess
{

class chess_game {
public:
	const position *current_position() const;
	side next_to_move() const;
	void make_move(move);
	void undo_move();
	void start();
	void stop();
	bool is_in_progress() const;
	unsigned get_move_counter() const;
	unsigned get_half_move_counter() const;
	void reset(std::string FEN);
	bool is_game_over() const;

	chess_game();
	chess_game(const chess_game &) = delete;
	chess_game(const chess_game &&) = delete;
	chess_game &operator=(const chess_game &) = delete;
	chess_game &operator=(const chess_game &&) = delete;

private:
	struct history_item {
		pmem::obj::persistent_ptr<history_item> previous;
		pmem::obj::p<position> pos;
		pmem::obj::p<move> next_move;
		pmem::obj::persistent_ptr<history_item> next;

		history_item(std::string FEN);
		history_item(pmem::obj::persistent_ptr<history_item>, move);
	};
	pmem::obj::persistent_ptr<history_item> head;
	pmem::obj::persistent_ptr<history_item> tail;

	pmem::obj::p<bool> is_game_in_progress;
};
}
}

#endif // EXAMPLES_PCHESS_CHESS_GAME_H
