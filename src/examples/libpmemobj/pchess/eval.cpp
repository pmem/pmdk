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
 * eval.cpp - implementation of static evaluation function
 */

#include "eval.hpp"
#include "position.hpp"

namespace examples
{
namespace pchess
{

/* All values expressed in centipawns */
constexpr int pawn_value = 100;
constexpr int bishop_value = 300;
constexpr int knight_value = 300;
constexpr int rook_value = 500;
constexpr int queen_value = 900;
constexpr int pawn_in_center_value = 10;
constexpr int pawn_shield_value = 10;

constexpr int square_attacked = 1;
constexpr int center_square_attacked = 2;
constexpr int pawn_defends_own_piece = 3;
constexpr int rook_opponent_home_attacked = 5;

static int
piece_value(enum piece p)
{
	switch (p) {
		case pawn:
			return pawn_value;
		case bishop:
			return bishop_value;
		case rook:
			return rook_value;
		case knight:
			return knight_value;
		case queen:
			return queen_value;
		default:
			return 0;
	}
}

static bool
is_center(int i)
{
	return i == sq_d4 or i == sq_d5 or i == sq_e4 or i == sq_e5;
}

static int
center_pawns(const chess_board &board)
{
	int value = 0;

	for (int i : {sq_d4, sq_d5, sq_e4, sq_e5}) {
		if (not board[i].is_empty and board[i].piece_type == pawn) {
			if (board[i].piece_side == white)
				value += pawn_in_center_value;
			else
				value -= pawn_in_center_value;
		}
	}

	return value;
}

static int
king_safety(const chess_board &board, int king_index, side who)
{
	int home_rank = (who == white) ? rank_1 : rank_8;
	int dir = (who == white) ? 1 : -1;
	int value;

	if (rank_at(king_index) != home_rank)
		return 0;

	value = 0;

	for (int d : {(int)north, north + east, north + west, north + north}) {
		auto sq = board[king_index + d * dir];

		if (sq.piece_type == pawn and sq.piece_side == who and
		    not sq.is_empty)
			value += pawn_shield_value;
	}

	return value;
}

static int
attack_scores(const position *pos, side who, int opponent_home_rank_0,
	      int opponent_home_rank_1)
{
	const chess_board &board = pos->get_board();
	int value = 0;

	for (int target : pos->get_attack_list(pawn, who)) {
		value += square_attacked;

		if (is_center(target))
			value += center_square_attacked;

		if (not board[target].is_empty and
		    board[target].piece_side == who)
			value += pawn_defends_own_piece;
	}

	for (int target : pos->get_attack_list(rook, who)) {
		value += square_attacked;

		if (rank_at(target) == opponent_home_rank_0 or
		    rank_at(target) == opponent_home_rank_1)
			value += rook_opponent_home_attacked;
	}

	for (auto p : {bishop, knight, queen}) {
		for (int target : pos->get_attack_list(p, who)) {
			value += square_attacked;

			if (is_center(target))
				value += center_square_attacked;
		}
	}

	return value;
}

int
eval(const position *pos)
{
	int value = 0;
	unsigned piece_count = 0;
	unsigned pawn_count = 0;
	const chess_board &board = pos->get_board();
	unsigned white_queen_count = 0;
	unsigned black_queen_count = 0;

	for (auto sq : board) {
		if (sq.is_empty)
			continue;

		++piece_count;

		if (sq.piece_type == pawn)
			++pawn_count;
		else if (sq.piece_type == queen and sq.piece_side == white)
			++white_queen_count;
		else if (sq.piece_type == queen and sq.piece_side == black)
			++black_queen_count;

		if (sq.piece_side == white)
			value += piece_value(sq.piece_type);
		else
			value -= piece_value(sq.piece_type);
	}

	if (piece_count > 20 and piece_count - pawn_count > 10)
		value += center_pawns(board);

	if (white_queen_count > 0) {
		value -= king_safety(board, pos->get_black_king_index(), black);
	}
	if (black_queen_count > 0) {
		value += king_safety(board, pos->get_white_king_index(), white);
	}

	value += attack_scores(pos, white, rank_7, rank_8);
	value -= attack_scores(pos, black, rank_2, rank_1);

	if (pos->get_side_to_move() == black)
		value = -value;

	return value;
}
}
}
