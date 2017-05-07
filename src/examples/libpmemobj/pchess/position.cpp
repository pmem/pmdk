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
 * position.cpp - implementation of the position class,
 * most basic chess rules are described here,
 * legal moves of pieces, etc..
 *
 * nothing pmem related
 */

#include "position.hpp"

#include <cctype>
#include <cstdlib>

using std::array;
using std::string;
using std::vector;
using std::string;

namespace examples
{
namespace pchess
{

string starting_FEN =
	"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

string
position::print_board() const
{
	int index = sq_a8;
	string result;

	result = "   A B C D E F G H\n";

	if (black_can_castle_queen_side)
		result += " q";
	else
		result += "  ";

	if (black_can_castle_king_side)
		result += "                 k";

	result += "\n";

	do {
		if (file_at(index) == file_a) {
			result += rank_to_char(rank_at(index));
			result += ' ';
		}
		result += '|';
		if (board[index].is_empty)
			result += ' ';
		else
			result += square_to_char(board[index]);

		index = east_of(index);
		if (is_on_east_edge(index)) {
			result += "| ";
			result += rank_to_char(rank_at(index));
			result += "\n";
			index = west_of(south_of(index), 8);
		}
	} while (not is_below_rank_1(index));

	if (white_can_castle_queen_side)
		result += " Q";
	else
		result += "  ";

	if (white_can_castle_king_side)
		result += "                 K";
	result += "\n   A B C D E F G H\n";

	return result;
}

side
position::get_side_to_move() const
{
	return side_to_move;
}

static const array<int, 4> bishop_directions = {
	{north + east, north + west, south + east, south + west}};
static const array<int, 4> rook_directions = {{north, south, east, west}};
static const array<int, 8> knight_moves = {
	{north + 2 * east, north + 2 * west, 2 * north + east, 2 * north + west,
	 south + 2 * east, south + 2 * west, 2 * south + east,
	 2 * south + west}};
static const array<int, 8> king_moves = {{north + west, north, north + east,
					  west, east, south + west, south,
					  south + east}};

bool
is_attacked(const chess_board &board, int where, side by)
{
	if (by == white) {
		if (board[east_of(south_of(where))] == white_pawn)
			return true;
		if (board[west_of(south_of(where))] == white_pawn)
			return true;
	} else {
		if (board[east_of(north_of(where))] == black_pawn)
			return true;
		if (board[west_of(north_of(where))] == black_pawn)
			return true;
	}

	for (int delta : king_moves) {
		auto sq = board[where + delta];

		if (sq.is_valid and not sq.is_empty and
		    sq.piece_type == king and sq.piece_side == by)
			return true;
	}

	for (int delta : knight_moves) {
		auto sq = board[where + delta];

		if (sq.is_valid and not sq.is_empty and
		    sq.piece_type == knight and sq.piece_side == by)
			return true;
	}

	for (int delta : bishop_directions) {
		int index = where + delta;
		while (board[index].is_valid and board[index].is_empty)
			index += delta;
		if (board[index].is_valid and board[index].piece_side == by and
		    (board[index].piece_type == bishop or
		     board[index].piece_type == queen))
			return true;
	}

	for (int delta : rook_directions) {
		int index = where + delta;
		while (board[index].is_valid and board[index].is_empty)
			index += delta;
		if (board[index].is_valid and board[index].piece_side == by and
		    (board[index].piece_type == rook or
		     board[index].piece_type == queen))
			return true;
	}

	return false;
}

/*
 * Conditionally add a move to a move_list
 * - check if king is left under attack after move.
 */
void
position::cadd_move(move m)
{
	chess_board new_board = board;
	int where;

	new_board[m.to] = new_board[m.from];
	new_board[m.from].is_empty = true;

	if (m.type == move_type::en_passant)
		new_board[en_passant_target].is_empty = true;

	if (board[m.from].piece_type == king)
		where = m.to;
	else if (side_to_move == white)
		where = white_king_index;
	else
		where = black_king_index;

	if (not is_attacked(new_board, where, opponent(side_to_move)))
		moves.push_back(m);
}

void
position::gen_non_sliding_moves(const array<int, 8> &deltas, int from)
{
	for (int delta : deltas) {
		if (board[from + delta].is_valid_target(side_to_move))
			cadd_move({from, from + delta});
	}
}

void
position::gen_pawn_moves(int from)
{
	int push, dpush, pawn_home_rank, last_rank;

	if (side_to_move == white) {
		push = from + north;
		dpush = push + north;
		pawn_home_rank = rank_2;
		last_rank = rank_7;
	} else {
		push = from + south;
		dpush = push + south;
		pawn_home_rank = rank_7;
		last_rank = rank_2;
	}

	if (board[push].is_empty) {
		if (rank_at(from) == last_rank) {
			cadd_move({from, push, move_type::promote_queen});
			cadd_move({from, push, move_type::promote_knight});
			cadd_move({from, push, move_type::promote_rook});
			cadd_move({from, push, move_type::promote_bishop});
		} else {
			cadd_move({from, push});
		}

		if (rank_at(from) == pawn_home_rank) {
			if (board[dpush].is_empty)
				cadd_move({from, dpush,
					   move_type::pawn_double_push});
		}
	}

	for (int to : {push + west, push + east}) {
		if (board[to].piece_side != side_to_move and
		    board[to].is_valid and not board[to].is_empty)
			cadd_move({from, to});
	}
}

void
position::gen_sliding_moves(const array<int, 4> &directions, int from)
{
	for (int delta : directions) {
		int to = from + delta;
		while (board[to].is_empty and board[to].is_valid) {
			cadd_move({from, to});
			to += delta;
		}

		if (board[to].is_valid_target(side_to_move))
			cadd_move({from, to});
	}
}

void
position::gen_en_passants()
{
	if (en_passant_target <= 0)
		return;

	int vdir = (side_to_move == white) ? 1 : -1;

	for (int hdir : {west, east}) {
		int from = en_passant_target + hdir;
		if (board[from].piece_side == side_to_move and
		    board[from].piece_type == pawn and not board[from].is_empty)
			cadd_move({from, en_passant_target + vdir * north,
				   move_type::en_passant});
	}
}

void
position::gen_castles()
{
	if (side_to_move == white and can_white_castle()) {
		if (is_attacked(board, sq_e1, black))
			return;

		if (white_can_castle_king_side and board[sq_f1].is_empty and
		    board[sq_g1].is_empty and
		    not is_attacked(board, sq_f1, black) and
		    not is_attacked(board, sq_g1, black))
			moves.push_back({sq_e1, sq_g1, move_type::castle});

		if (white_can_castle_queen_side and board[sq_b1].is_empty and
		    board[sq_c1].is_empty and board[sq_d1].is_empty and
		    not is_attacked(board, sq_d1, black) and
		    not is_attacked(board, sq_c1, black))
			moves.push_back({sq_e1, sq_c1, move_type::castle});
	} else if (side_to_move == black and can_black_castle()) {
		if (is_attacked(board, sq_e8, white))
			return;

		if (black_can_castle_king_side and board[sq_f8].is_empty and
		    board[sq_g8].is_empty and
		    not is_attacked(board, sq_f8, white) and
		    not is_attacked(board, sq_g8, white))
			moves.push_back({sq_e8, sq_g8, move_type::castle});

		if (black_can_castle_queen_side and board[sq_b8].is_empty and
		    board[sq_c8].is_empty and board[sq_d8].is_empty and
		    not is_attacked(board, sq_d8, white) and
		    not is_attacked(board, sq_c8, white))
			moves.push_back({sq_e8, sq_c8, move_type::castle});
	}
}

void
position::generate_moves()
{
	for (decltype(board.size()) from = 0; from < board.size(); ++from) {
		square sq = board[from];

		if (sq.is_empty or sq.piece_side != side_to_move)
			continue;

		switch (sq.piece_type) {
			case knight:
				gen_non_sliding_moves(knight_moves, from);
				break;
			case king:
				gen_non_sliding_moves(king_moves, from);
				break;
			case bishop:
				gen_sliding_moves(bishop_directions, from);
				break;
			case rook:
				gen_sliding_moves(rook_directions, from);
				break;
			case queen:
				gen_sliding_moves(rook_directions, from);
				gen_sliding_moves(bishop_directions, from);
				break;
			case pawn:
				gen_pawn_moves(from);
				break;
		}
	}

	gen_en_passants();
	gen_castles();
}

/*
 * Utility functions for parsing FEN strings.
 *
 * "rnbqkbnr/pp1ppppp/8/2p5/4P3/8/PPPP1PPP/RNBQKBNR   w   KQkq   c6   0   2"
 *    |                                               |   |      |    |   |
 *  FEN_parse_board              FEN_parse_side_to_move   |      |    |   |
 *  FEN_print_board              FEN_print_side_to_move   |      |    |   |
 *                                                        |      |    |   |
 *                                  FEN_parse_castle_rights      |    |   |
 *                                  FEN_print_castle_rights      |    |   |
 *                                                               |    |   |
 *                                     FEN_parse_en_passant_square    |   |
 *                                     FEN_print_en_passant_square    |   |
 *                                                                    |   |
 *                                                    half_move_counter   |
 *                                                                        |
 *                                                        full_move_counter
 */

static void
FEN_parse_board(string::const_iterator &c, chess_board &board)
{
	int index = sq_a8;

	do {
		do {
			if (is_char_piece(*c)) {
				board[index] = char_to_square(*c);
				index = east_of(index);
			} else if (std::isdigit(*c)) {
				for (char t = *c; t > '0'; --t) {
					if (is_on_east_edge(index))
						throw;
					index = east_of(index);
				}
			} else {
				throw;
			}

			c++;
		} while (not is_on_east_edge(index));

		index = west_of(south_of(index), 8);
		if (is_below_rank_1(index))
			return;
		if (*c != '/')
			throw;
		c++;
	} while (true);
}

static void
FEN_skip_ws(string::const_iterator &c)
{
	if (not std::isspace(*c))
		throw;

	while (std::isspace(*c))
		++c;
}

static side
FEN_parse_side_to_move(string::const_iterator &c)
{
	switch (std::tolower(*c++)) {
		case 'w':
			return white;
		case 'b':
			return black;
		default:
			throw;
	}
}

static void
FEN_parse_castle_rights(string::const_iterator &c,
			bool &white_can_castle_king_side,
			bool &white_can_castle_queen_side,
			bool &black_can_castle_king_side,
			bool &black_can_castle_queen_side)
{
	if (*c == '-') {
		c++;
		return;
	}

	do {
		switch (*c) {
			case 'K':
				if (white_can_castle_king_side)
					throw;
				white_can_castle_king_side = true;
				break;
			case 'Q':
				if (white_can_castle_queen_side)
					throw;
				white_can_castle_queen_side = true;
				break;
			case 'k':
				if (black_can_castle_king_side)
					throw;
				black_can_castle_king_side = true;
				break;
			case 'q':
				if (black_can_castle_queen_side)
					throw;
				black_can_castle_queen_side = true;
				break;
		}

		c++;
	} while (not isspace(*c));
}

static int
FEN_parse_en_passant_square(string::const_iterator &c, side to_move)
{
	int index;

	if (*c == '-') {
		c++;
		return -1;
	}

	index = parse_coordinates(c);

	if (to_move == white and rank_at(index) != rank_6)
		throw;

	if (to_move == black and rank_at(index) != rank_3)
		throw;

	c += 2;

	return index;
}

static unsigned
FEN_parse_move_count(string::const_iterator &c)
{
	unsigned long count;
	string count_string;
	size_t pos;

	while (std::isdigit(*c))
		count_string += *c++;

	count = std::stoul(count_string, &pos, 10);

	if (count > 8192)
		throw;

	return count;
}

static void
setup_king_index(const chess_board &board, int &white_king_index,
		 int &black_king_index)
{
	white_king_index = -1;
	black_king_index = -1;

	for (size_t i = 0; i < board.size(); i++) {
		if (board[i] == white_king) {
			if (white_king_index != -1)
				throw;

			white_king_index = i;
		}
		if (board[i] == black_king) {
			if (black_king_index != -1)
				throw;

			black_king_index = i;
		}
	}
}

position::position(string FEN)
{
	auto c = FEN.cbegin();

	clear();

	while (std::isspace(*c))
		++c;

	FEN_parse_board(c, board);
	FEN_skip_ws(c);
	side_to_move = FEN_parse_side_to_move(c);
	FEN_skip_ws(c);
	FEN_parse_castle_rights(
		c, white_can_castle_king_side, white_can_castle_queen_side,
		black_can_castle_king_side, black_can_castle_queen_side);
	FEN_skip_ws(c);
	en_passant_target = FEN_parse_en_passant_square(c, side_to_move);
	FEN_skip_ws(c);
	half_move_counter = FEN_parse_move_count(c);
	FEN_skip_ws(c);
	full_move_counter = FEN_parse_move_count(c);
	if (full_move_counter == 0)
		throw;
	setup_king_index(board, white_king_index, black_king_index);
	is_king_attacked =
		is_attacked(board, (side_to_move == white) ? white_king_index
							   : black_king_index,
			    opponent(side_to_move));
	generate_moves();
	update_attack_lists();
}

void
position::clear()
{
	for (decltype(board.size()) i = 0; i < board.size(); ++i) {
		board[i].is_valid = not is_on_edge(i);
		board[i].is_empty = true;
	}

	white_can_castle_king_side = false;
	white_can_castle_queen_side = false;
	black_can_castle_king_side = false;
	black_can_castle_queen_side = false;
	en_passant_target = -1;
	side_to_move = white;
	half_move_counter = 0;
	full_move_counter = 1;
}

static void
FEN_print_board(string &result, const chess_board &board)
{
	int index = sq_a8;
	int empty_count = 0;

	do {
		if (board[index].is_empty) {
			++empty_count;
		} else {
			if (empty_count != 0) {
				result += std::to_string(empty_count);
				empty_count = 0;
			}
			result += square_to_char(board[index]);
		}

		index = east_of(index);
		if (is_on_east_edge(index)) {
			if (empty_count != 0) {
				result += std::to_string(empty_count);
				empty_count = 0;
			}

			index = west_of(south_of(index), 8);
			if (is_below_rank_1(index))
				return;

			result += '/';
		}
	} while (true);
}

static void
FEN_print_side_to_move(string &result, side to_move)
{
	if (to_move == white)
		result += 'w';
	else
		result += 'b';
}

static void
FEN_print_castle_rights(string &result, bool white_can_castle_king_side,
			bool white_can_castle_queen_side,
			bool black_can_castle_king_side,
			bool black_can_castle_queen_side)
{
	if (white_can_castle_king_side)
		result += 'K';
	if (white_can_castle_queen_side)
		result += 'Q';
	if (black_can_castle_king_side)
		result += 'k';
	if (black_can_castle_queen_side)
		result += 'q';

	if (std::isspace(result.back()))
		result += '-';
}

static void
FEN_print_en_passant_square(string &result, int index)
{
	if (index < 0)
		result += '-';
	else
		result += print_coordinates(index);
}

string
position::print_FEN() const
{
	string result;

	FEN_print_board(result, board);
	result += ' ';
	FEN_print_side_to_move(result, side_to_move);
	result += ' ';
	FEN_print_castle_rights(
		result, white_can_castle_king_side, white_can_castle_queen_side,
		black_can_castle_king_side, black_can_castle_queen_side);
	result += ' ';
	FEN_print_en_passant_square(result, en_passant_target);
	result += ' ';
	result += std::to_string(half_move_counter);
	result += ' ';
	result += std::to_string(full_move_counter);

	return result;
}

bool
position::is_move_reversible(move m) const
{
	if (not board[m.to].is_empty)
		return false;
	if (board[m.from].piece_type == pawn)
		return false;
	if (m.from == sq_e1 and can_white_castle())
		return false;
	if (m.from == sq_e8 and can_black_castle())
		return false;
	if (m.from == sq_a1 and white_can_castle_queen_side)
		return false;
	if (m.from == sq_h1 and white_can_castle_king_side)
		return false;
	if (m.from == sq_a8 and black_can_castle_queen_side)
		return false;
	if (m.from == sq_h8 and black_can_castle_king_side)
		return false;
	if (m.to == sq_a1 and white_can_castle_queen_side)
		return false;
	if (m.to == sq_h1 and white_can_castle_king_side)
		return false;
	if (m.to == sq_a8 and black_can_castle_queen_side)
		return false;
	if (m.to == sq_h8 and black_can_castle_king_side)
		return false;
	return true;
}

position
position::make_move(move m) const
{
	position child = *this;
	child.moves.count = 0;

	child.board[m.to] = child.board[m.from];
	child.board[m.from].is_empty = true;

	child.side_to_move = (side_to_move == white) ? black : white;

	child.en_passant_target = -1;
	switch (m.type) {
		case move_type::pawn_double_push:
			child.en_passant_target = m.to;
			break;
		case move_type::en_passant:
			child.board[en_passant_target].is_empty = true;
			break;
		case move_type::promote_queen:
			child.board[m.to].piece_type = queen;
			break;
		case move_type::promote_knight:
			child.board[m.to].piece_type = knight;
			break;
		case move_type::promote_rook:
			child.board[m.to].piece_type = rook;
			break;
		case move_type::promote_bishop:
			child.board[m.to].piece_type = bishop;
			break;
		default:
			break;
	}

	if (board[m.from] == white_king) {
		child.white_king_index = m.to;
		child.white_can_castle_king_side = false;
		child.white_can_castle_queen_side = false;
		if (m.type == move_type::castle and m.to == sq_c1) {
			child.board[sq_d1] = child.board[sq_a1];
			child.board[sq_a1].is_empty = true;
		}
		if (m.type == move_type::castle and m.to == sq_g1) {
			child.board[sq_f1] = child.board[sq_h1];
			child.board[sq_h1].is_empty = true;
		}
	}

	if (m.from == sq_a1 or m.to == sq_a1)
		child.white_can_castle_queen_side = false;
	if (m.from == sq_h1 or m.to == sq_h1)
		child.white_can_castle_king_side = false;

	if (board[m.from] == black_king) {
		child.black_king_index = m.to;
		child.black_can_castle_king_side = false;
		child.black_can_castle_queen_side = false;
		if (m.type == move_type::castle and m.to == sq_c8) {
			child.board[sq_d8] = child.board[sq_a8];
			child.board[sq_a8].is_empty = true;
		}
		if (m.type == move_type::castle and m.to == sq_g8) {
			child.board[sq_f8] = child.board[sq_h8];
			child.board[sq_h8].is_empty = true;
		}
	}

	if (m.from == sq_a8 or m.to == sq_a8)
		child.black_can_castle_queen_side = false;
	if (m.from == sq_h8 or m.to == sq_h8)
		child.black_can_castle_king_side = false;

	if (side_to_move == black)
		child.full_move_counter++;

	if (is_move_reversible(m))
		child.half_move_counter++;
	else
		child.half_move_counter = 0;

	child.is_king_attacked = is_attacked(
		child.board, (side_to_move == white) ? child.black_king_index
						     : child.white_king_index,
		side_to_move);

	child.generate_moves();
	child.update_attack_lists();

	return child;
}

string
position::print_move(move m) const
{
	string result;

	result += print_coordinates(m.from);
	result += print_coordinates(m.to);

	if (m.type == move_type::promote_queen)
		result += 'q';
	else if (m.type == move_type::promote_rook)
		result += 'r';
	else if (m.type == move_type::promote_bishop)
		result += 'b';
	else if (m.type == move_type::promote_knight)
		result += 'n';

	return result;
}

move
position::parse_move(string str) const
{
	for (move m : moves) {
		string item = print_move(m);
		if (item == str)
			return m;
	}

	return null_move;
}

const chess_board &
position::get_board() const
{
	return board;
}

bool
position::is_checkmate() const
{
	return moves.count == 0 and is_king_attacked;
}

bool
position::is_stalemate() const
{
	return moves.count == 0 and not is_king_attacked;
}

void
position::add_white_pawn_attacks(int from)
{
	if (board[from + north + east].is_valid)
		white_attacks[pawn].push(from + north + east);
	if (board[from + north + west].is_valid)
		white_attacks[pawn].push(from + north + west);
}

void
position::add_black_pawn_attacks(int from)
{
	if (board[from + south + east].is_valid)
		white_attacks[pawn].push(from + south + east);
	if (board[from + south + west].is_valid)
		white_attacks[pawn].push(from + south + west);
}

void
position::add_non_sliding_attacks(int from, const std::array<int, 8> &dirs)
{
	for (int dir : dirs) {
		int to = from + dir;

		if (board[to].is_valid) {
			if (board[from].piece_side == white)
				white_attacks[board[from].piece_type].push(to);
			else
				black_attacks[board[from].piece_type].push(to);
		}
	}
}

void
position::add_sliding_attacks(int from, const std::array<int, 4> &dirs)
{
	for (int dir : dirs) {
		int to = from + dir;

		while (board[to].is_valid) {
			if (board[from].piece_side == white)
				white_attacks[board[from].piece_type].push(to);
			else
				black_attacks[board[from].piece_type].push(to);

			if (not board[to].is_empty)
				break;

			to += dir;
		}
	}
}

void
position::update_attack_lists()
{
	for (auto &list : white_attacks)
		list.count = 0;

	for (auto &list : black_attacks)
		list.count = 0;

	for (decltype(board.size()) from = 0; from < board.size(); ++from) {
		square sq = board[from];

		if (sq.is_empty)
			continue;

		switch (sq.piece_type) {
			case pawn:
				if (sq.piece_side == white)
					add_white_pawn_attacks(from);
				else
					add_black_pawn_attacks(from);
				break;
			case king:
				add_non_sliding_attacks(from, king_moves);
				break;
			case bishop:
				add_sliding_attacks(from, bishop_directions);
				break;
			case rook:
				add_sliding_attacks(from, rook_directions);
				break;
			case queen:
				add_sliding_attacks(from, rook_directions);
				add_sliding_attacks(from, bishop_directions);
				break;
			case knight:
				add_non_sliding_attacks(from, knight_moves);
				break;
		}
	}
}

const attack_list &
position::get_attack_list(piece p, side s) const
{
	if (s == white)
		return white_attacks[p];
	else
		return black_attacks[p];
}
}
}
