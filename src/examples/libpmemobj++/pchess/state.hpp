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
 * state.hpp - The declaration of the type used to hold the global state.
 */

#ifndef EXAMPLES_PCHESS_STATE_H
#define EXAMPLES_PCHESS_STATE_H

#include <chrono>
#include <iostream>

#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>

#include "chess_game.hpp"
#include "search.hpp"

namespace examples
{
namespace pchess
{

class state {
public:
	state();

	/*
	 * iterate_main_loop(input, output, err)
	 * pchess runs on a single thread (using pmem and multiple
	 * threads seemed too complicated for my first pmem aware application).
	 * Everything happens in a sort of 'event loop', where each loop
	 * iteration is expected to be in a pmem transaction.
	 */
	void iterate_main_loop(std::istream &input, std::ostream &output,
			       std::ostream &output_error);

	/*
	 * is_finished()
	 * Is the operator finished with the game?
	 * This is only indicated with the "quit" command by the operator.
	 * Data can be cleared from persistent memory, a new game
	 * shell be started next time, with a reinitialized root object.
	 */
	bool is_finished() const;

	/*
	 * is_session_finished()
	 * The user has quit the program, or just input is no
	 * longer available.
	 * pchess can close the pmem pool, and stop the process.
	 */
	bool is_session_finished() const;

private:
	chess_game game;
	search searcher;

	enum action_enum { a_none, a_accept_input, a_analyze, a_think, a_eol };

	pmem::obj::p<action_enum> next_action;

	/*
	 * search resolution
	 * The number of nodes to search in each transaction.
	 * Output is refreshed after each transaction, thus
	 * each time search_resolution number of nodes searched.
	 */
	pmem::obj::p<unsigned> search_resolution;

	/*
	 * node_count
	 * The number of nodes searched while thinking on the current move,
	 * Or during an 'analyze' search. Increasing monotonically during IID,
	 * and is reset to zero on each move.
	 */
	pmem::obj::p<unsigned long> node_count;

	/*
	 * search_time
	 * The time spent thinking about the next move,
	 * or in analyze mode.
	 */
	pmem::obj::p<std::chrono::milliseconds> search_time;

	/*
	 * time_per_move
	 * The time the computer is allowed to think on each move.
	 * The operator's time for thinking is not measured.
	 */
	pmem::obj::p<std::chrono::milliseconds> time_per_move;

	/*
	 * Store the result of the last completed search.
	 */
	pmem::obj::p<bool> has_known_result;
	pmem::obj::p<int> last_known_value;
	pmem::obj::p<int> last_known_value_depth;
	pmem::obj::p<move_list> last_known_PV;

	/*
	 * nps - A temporary value indicating the speed of the search,
	 * measured in "Nodes searched Per Second". No need to
	 * save this value in persistent memory.
	 */
	unsigned long nps;

	pmem::obj::p<bool> board_on_move;

	pmem::obj::p<side> computer_side;

	/*
	 * Some methods to contain implementation details.
	 * Header files could be simpler if pmemobj supported
	 * polymorphic classes, but that is not the case at the
	 * time writing this code.
	 */
	void dispatch_command(std::string command, std::istream &input,
			      std::ostream &output);
	void continue_search(std::ostream &output);
	const position *current_position() const;
	void cmd_set_board(std::istream &input);
	unsigned long cmd_perft(std::istream &input);
	void cmd_divide(std::istream &input, std::ostream &output);
	void cmd_force();
	void cmd_new();
	void cmd_go();
	void cmd_analyze(std::istream &input);
	void operator_move(std::ostream &, move);
	void print_game_result(std::ostream &) const;
	void print_search_stats(std::ostream &) const;
	void print_PV(std::ostream &) const;
	void reset_search_stats();
	void analyze(std::ostream &);
	void think(std::ostream &output);
	void start_thinking();
	std::chrono::milliseconds time_left() const;
	bool mate_found() const;
	void cmd_printboard(std::ostream &);
};
}
}

#endif // EXAMPLES_PCHESS_STATE_H
