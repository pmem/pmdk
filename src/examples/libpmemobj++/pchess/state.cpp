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
 * state.cpp - Implementing a god object, overseeing
 * the global state of a pchess process.
 */

#include "state.hpp"
#include "eval.hpp"
#include "search.hpp"

#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pext.hpp>

using std::string;
using std::istream;
using std::ostream;
using pmem::obj::persistent_ptr;
using trans = pmem::obj::transaction;
using steady_clock = std::chrono::steady_clock;
using namespace std::chrono;
using namespace std::chrono_literals;

namespace examples
{
namespace pchess
{

static const string help_general =
	"Please choose one of these help sections:\n"
	"help play -- introduction, how to play a quick game\n"
	"help new -- the \"new\" command\n"
	"help go -- the \"go\" command\n"
	"help force -- the \"force\" command\n"
	"help debug -- the commands related to debugging the engine\n"
	"help notation -- about chess notations used\n"
	"help commands -- list all known commands\n";

static const string help_debug =
	"The following commands are mainly used for debugging:\n"
	"\n"
	"analyze ply - run a negamax search, with the given depth\n"
	"              note: this differs from the analyze command\n"
	"              in the xboard protocol\n"
	"perft ply - compute perft value corresponding to the current\n"
	"            position, and the given depth\n"
	"divide ply - compute perft value corresponding to the direct\n"
	"             children of the current position, at ply-1 depth\n"
	"             for each of them.\n";

static const string help_play =
	" To play a game, the operator needs to type his/her moves\n"
	" in coordinate notation, and wait for the engine to reply\n"
	" its own moves:\n"
	"\n"
	" operator: e2e4\n"
	" engine: thinking....\n"
	" engine: e7e5....\n"
	" etc...\n"
	"\n"
	" As some help, the engine can print a visual representation\n"
	" of the current board. This is controlled by a flag, which can\n"
	" be toggled using the boardonmove command\n"
	"\n"
	" Upon starting a new game, the engine is set to play black,\n"
	" but the operator can easily switch sides using the go command.\n"
	" If the go command is issued while it is white's turn to move,\n"
	" the engine goes on thinking about white's move, and operator\n"
	" plays black form than on.\n"
	"\n"
	" Once the game is over, the engine enters the 'force' mode, and a\n"
	" new game can of course be started with the new command.\n"
	" Alternatively, one can set up a custom starting position in force\n"
	" mode, using the setboard command, and then start a game from that\n"
	" position using the go command\n"
	"\n"
	" Of course the process can be terminated any time during the game,\n"
	" and next time pchess is started with the same pmem pool, it just\n"
	" continues from the same point in the game.\n"
	"\n"
	" Have fun!\n";

static const string help_go =
	"go - set the engine to play the side next to move,\n"
	"     and start thinking. If the it was the operator's turn to move,\n"
	"     this effectively exchanges the sides the two players play.\n";

static const string help_new =
	"new - Start a new game.\n"
	"      Setup the regulat chess starting position, and assign the\n"
	"      engine to play black. The engine than waits for the users\n"
	"      to make the first move as white\n";

static const string help_force =
	"force - stop the game, if one is in progress, and enter force mode.\n"
	"        In this mode, the operator can make moves for both players,\n"
	"        or setup a new position with the setboard command.\n";

static const string help_notation =
	" move notations\n"
	" As of this version, pchess only understands moves\n"
	" in a simple coordinate notation, that is:\n"
	" File and rank of originating square followed by the file\n"
	" and rank of the moves target square.\n"
	" Promotions are specified by a letter following the coordinates,\n"
	" one of 'q', 'r', 'b', or 'n'.\n Castling moves are described using\n"
	" the kings source and desination squares. Examples:\n"
	" a pawn push: e2e4\n"
	" an en-passant capture: e4d3\n"
	" a promotion to bishop: c7c8b\n"
	" black castling queenside: e8c8\n"
	"\n"
	" position notation\n"
	" pchess understands FEN, see:\n"
	" http://en.wikipedia.org/wiki/Forsyth%E2%80%93Edwards_Notation\n"
	"\n"
	" ply\n"
	" See: https://en.wikipedia.org/wiki/Ply_(game_theory)\n"
	"\n"
	" eval, value\n"
	" See: https://en.wikipedia.org/wiki/Evaluation_function\n"
	" Heuristic score computer for a position, from the point of view of"
	" the player to move, meauserd in pawns.\n"
	" e.g.: in the thinking output: \"value: 1.04\" means the engine"
	" thinks it is a pawn up, \"value: -5.50\" means the engine"
	" thinks it is five pawns down -- maybe lost a rook.\n"
	"\n"
	" node\n"
	" A chess position, or a node in the game-tree.\n"
	"\n"
	" nps\n"
	" A measure of search speed, nodes per second.\n";

static const string help_commands =
	"List of commands:\n"
	"printfen - print the FEN of the current position\n"
	"printboard - print a table representing the board\n"
	"setboard FEN - setup a new position, using a FEN string\n"
	"new - start a new game -- see: help new\n"
	"go - start engine thinking -- see: help go\n"
	"force - stop the current game -- see help force\n"
	"quit - quit pchess, forget current game\n"
	"boardonmove - print board after each move made\n"
	"analyze depth - see: help debug\n"
	"perft depth - see: help debug\n"
	"divide depth - see: help debug\n";

constexpr auto search_time_resolution = 20ms;

state::state()
    : next_action(a_accept_input),
      search_resolution(4),
      time_per_move(5s),
      board_on_move(false),
      computer_side(BLACK)
{
	game.start();
}

const position *
state::current_position() const
{
	return game.current_position();
}

void
state::cmd_set_board(istream &input)
{
	string FEN;

	std::getline(input, FEN);
	game.reset(FEN);
}

unsigned long
state::cmd_perft(istream &input)
{
	unsigned depth;

	input >> depth;
	return perft(current_position(), depth);
}

void
state::cmd_divide(istream &input, ostream &output)
{
	auto moves = current_position()->get_moves();

	unsigned depth;

	input >> depth;

	if (depth == 0)
		return;

	for (move m : moves) {
		position child = current_position()->make_move(m);
		output << current_position()->print_move(m) << " "
		       << perft(&child, depth - 1) << std::endl;
	}
}

void
state::cmd_force()
{
	game.stop();
}

void
state::start_thinking()
{
	reset_search_stats();
	next_action = a_think;
	searcher.reset(*current_position(), 1);
}

milliseconds
state::time_left() const
{
	return time_per_move.get_ro() - search_time.get_ro();
}

bool
state::mate_found() const
{
	if (not has_known_result)
		return false;

	return last_known_value >= mate_value or
		last_known_value <= -mate_value;
}

void
state::think(ostream &output)
{
	continue_search(output);

	if (time_left() <= search_time_resolution or mate_found()) {
		move m;

		if (has_known_result) {
			m = last_known_PV.get_ro()[0];
		} else {
			move_list moves = current_position()->get_moves();
			m = moves[0];
		}

		output << "\ncomputers move: "
		       << current_position()->print_move(m) << "\n";
		game.make_move(m);

		if (board_on_move)
			cmd_printboard(output);

		if (game.is_game_over())
			print_game_result(output);

		output.flush();
		next_action = a_accept_input;
	} else if (searcher.is_done()) {
		searcher.reset(*current_position(), last_known_value_depth + 1);
	}
}

void
state::cmd_new()
{
	game.reset(starting_FEN);
	computer_side = BLACK;
	game.start();
}

void
state::cmd_go()
{
	game.start();
	computer_side = game.next_to_move();
	start_thinking();
}

void
state::print_game_result(ostream &output) const
{
	if (current_position()->is_checkmate())
		output << "checkmate\n";
	else
		output << "stalemate\n";
}

void
state::cmd_printboard(ostream &output)
{
	output << current_position()->print_board();
}

void
state::operator_move(ostream &output, move m)
{
	if (game.is_in_progress()) {
		game.make_move(m);

		if (board_on_move)
			cmd_printboard(output);

		if (not game.is_game_over())
			start_thinking();
		else
			print_game_result(output);
	} else {
		game.make_move(m);
	}
}

void
state::cmd_analyze(istream &input)
{
	unsigned depth;

	/*
	 * Tried this first:
	 *
	 * if (not (input >> depth))
	 *
	 * but I gave up, and clang-format won.
	 */
	if (!(input >> depth))
		throw std::invalid_argument("invalid number");

	if (depth == 0)
		return;

	next_action = a_analyze;
	reset_search_stats();
	searcher.reset(*current_position(), depth);
}

void
state::reset_search_stats()
{
	search_time = 0ms;
	nps = 0;
	node_count = 0;
	has_known_result = false;
	last_known_value_depth = 0;
}

static void
cmd_help(istream &input, ostream &output)
{
	string section;

	if (input >> section) {
		if (section == "debug")
			output << help_debug;
		else if (section == "play")
			output << help_play;
		else if (section == "new")
			output << help_new;
		else if (section == "go")
			output << help_go;
		else if (section == "force")
			output << help_force;
		else if (section == "notation")
			output << help_notation;
		else if (section == "commands")
			output << help_commands;
		else
			output << "unknown help section\n\n" << help_general;
	} else {
		output << help_general;
	}
}

void
state::dispatch_command(std::string command, istream &input, ostream &output)
{
	if (not game.is_in_progress() or game.next_to_move() != computer_side) {
		move m = current_position()->parse_move(command);

		if (not m.is_null()) {
			operator_move(output, m);
			return;
		}
	}

	if (command == "printfen")
		output << current_position()->print_FEN() << std::endl;
	else if (command == "help" or command == "h" or command == "H")
		cmd_help(input, output);
	else if (command == "setboard")
		cmd_set_board(input);
	else if (command == "printboard")
		cmd_printboard(output);
	else if (command == "perft")
		output << cmd_perft(input) << std::endl;
	else if (command == "divide")
		cmd_divide(input, output);
	else if (command == "force")
		cmd_force();
	else if (command == "new")
		cmd_new();
	else if (command == "go")
		cmd_go();
	else if (command == "analyze")
		cmd_analyze(input);
	else if (command == "quit")
		next_action = a_none;
	else if (command == "boardonmove")
		board_on_move = not board_on_move;
	else
		throw std::invalid_argument("unknown command");
}

static void
print_node_count(ostream &output, unsigned long value)
{
	if (value > 1000) {
		if (value > 1000000) {
			output << (value / 1000000) << "."
			       << ((value % 1000000) / 100000) << "M";
		} else {
			output << (value / 1000) << "."
			       << ((value % 1000) / 100) << "K";
		}
	} else {
		output << value;
	}
}

static void
print_ms(ostream &output, milliseconds value)
{
	if (value.count() > 60000) {
		output << (value.count() / 60000) << "m";
		output << std::setfill('0') << std::setw(2)
		       << ((value.count() % 60000) / 1000) << "s";
	} else {
		output << (value.count() / 1000) << ".";
		output << std::setfill('0') << std::setw(2)
		       << ((value.count() % 1000) / 10);
		output << "s";
	}
}

void
state::continue_search(ostream &output)
{
	auto start_time = steady_clock::now();
	auto prev_node_count = searcher.get_node_count();

	searcher.negamax(search_resolution);

	auto search_time_increase =
		duration_cast<milliseconds>(steady_clock::now() - start_time);
	auto node_count_delta = searcher.get_node_count() - prev_node_count;
	node_count = node_count + node_count_delta;
	search_time = search_time.get_ro() + search_time_increase;
	if (search_time_increase > 0ms) {
		nps = (1000 * node_count_delta) / search_time_increase.count();

		if (node_count_delta == search_resolution) {
			if (search_time_increase >
			    search_time_resolution + 2ms) {
				search_resolution -= search_resolution / 10;
			} else if (search_time_increase <
				   search_time_resolution - 2ms) {
				search_resolution += search_resolution / 10;
			}
		}
	} else {
		nps = 0;
		if (node_count_delta == search_resolution)
			search_resolution = search_resolution * 8;
	}

	if (searcher.is_done()) {
		last_known_value = searcher.get_value();
		last_known_PV = searcher.get_PV();
		last_known_value_depth = searcher.get_depth();
		has_known_result = true;
	}

	output << "                                              \r";
	print_search_stats(output);
	print_PV(output);
	output.flush();
}

void
state::print_search_stats(ostream &output) const
{
	output << "nodes: ";
	print_node_count(output, node_count);
	output << "   time: ";
	print_ms(output, search_time);
	output << "   nps: ";
	print_node_count(output, nps);
}

static void
print_centipawns(ostream &output, int value)
{
	if (value >= mate_value) {
		output << "+inf";
	} else if (value <= -mate_value) {
		output << "-inf";
	} else {
		if (value < 0) {
			value = -value;
			output << "-";
		}

		output << (value / 100) << ".";
		output << std::setfill('0') << std::setw(2) << (value % 100);
	}
}

void
state::print_PV(ostream &output) const
{
	if (not has_known_result)
		return;

	output << "   value: ";
	print_centipawns(output, last_known_value);
	output << "   depth: " << last_known_value_depth << " ply   PV: ";
	position t = *current_position();

	for (auto m : last_known_PV.get_ro()) {
		output << t.print_move(m) << " ";
		t = t.make_move(m);
	}
}

bool
state::is_session_finished() const
{
	return next_action == a_none or next_action == a_eol;
}

bool
state::is_finished() const
{
	return next_action == a_none;
}

void
state::analyze(ostream &output)
{
	continue_search(output);

	if (searcher.is_done()) {
		next_action = a_accept_input;
		output << "\n";
		output.flush();
	}
}

void
state::iterate_main_loop(istream &input, ostream &output, ostream &output_error)
{
	switch (next_action) {
		case a_analyze:
			analyze(output);
			break;
		case a_think:
			think(output);
			break;
		case a_eol:
			next_action = a_accept_input;
		//      [[fallthrough]];
		case a_accept_input: {
			string line;

			if (not std::getline(input, line)) {
				next_action = a_eol;
				return;
			}

			string command;
			std::stringstream line_stream(line);
			if (line_stream >> command) {
				try {
					dispatch_command(command, line_stream,
							 output);
				} catch (const std::exception &e) {
					output_error << e.what() << std::endl;
				}
			}
			break;
		}
		default:
			break;
	}
}
}
}
