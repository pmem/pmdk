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
 * position.hpp - A class to represent a chess position, including
 * the pieces on the board, and few auxiliary information. This information
 * should be enough to make most decisions about the position, except
 * regarding draw by repetition.
 *
 * nothing pmem related here
 */

#ifndef EXAMPLES_PCHESS_POSITION_H
#define EXAMPLES_PCHESS_POSITION_H

#include "chess.hpp"

#include <array>
#include <string>
#include <vector>

namespace examples
{
namespace pchess
{

/*
 * https://chessprogramming.wikispaces.com/10x12+Board
 * The most simple representation, good for educational programs.
 * Note: the three-fold repetition rule is ignored here for the
 * sake of simplicity. The fifty move rule can be checked, using the
 * full_move counter.
 */
typedef std::array<square, 120> chess_board;

bool is_attacked(const chess_board &, int where, side by);

/*
 * A struct with no pointers, as at the writing of this, this is the
 * easiest way to store a list of objects.
 * Containers such as std::vector<> can not yet be used with NVML.
 */
struct attack_list {
	unsigned count;
	std::array<int, 64> targets;

	void
	push(int target)
	{
		for (unsigned i = 0; i < count; ++i) {
			if (targets[i] == target)
				return;
		}

		targets[count++] = target;
	}

	const int *
	begin() const
	{
		return &targets[0];
	}

	const int *
	end() const
	{
		return &targets[count];
	}
};

class position {
public:
	std::string print_board() const;

	// https://chessprogramming.wikispaces.com/Forsyth-Edwards+Notation
	std::string print_FEN() const;
	position(std::string FEN);

	position make_move(move) const;
	std::string print_move(move) const;
	move parse_move(std::string) const;
	bool is_checkmate() const;
	bool is_stalemate() const;

	const move_list &
	get_moves() const
	{
		return moves;
	}

	side get_side_to_move() const;

	bool is_move_reversible(move) const;

	square square_at(int index) const;
	const chess_board &get_board() const;

	int
	get_white_king_index() const
	{
		return white_king_index;
	}

	int
	get_black_king_index() const
	{
		return black_king_index;
	}

	bool
	is_in_check() const
	{
		return is_king_attacked;
	}

	const attack_list &get_attack_list(piece, side) const;

	bool
	can_white_castle() const
	{
		return white_can_castle_king_side or
			white_can_castle_queen_side;
	}

	bool
	can_black_castle() const
	{
		return black_can_castle_king_side or
			black_can_castle_queen_side;
	}

	bool
	can_white_castle_kingside() const
	{
		return white_can_castle_king_side;
	}

	bool
	can_white_castle_queenside() const
	{
		return white_can_castle_queen_side;
	}

	bool
	can_black_castle_kingside() const
	{
		return black_can_castle_king_side;
	}

	bool
	can_black_castle_queenside() const
	{
		return black_can_castle_queen_side;
	}

private:
	side side_to_move;
	chess_board board;
	int en_passant_target;
	bool white_can_castle_king_side;
	bool white_can_castle_queen_side;
	bool black_can_castle_king_side;
	bool black_can_castle_queen_side;
	unsigned full_move_counter;
	unsigned half_move_counter;
	int white_king_index;
	int black_king_index;

	/*
	 * Utility methods used while move generation.
	 */
	void cadd_move(move);
	void gen_non_sliding_moves(const std::array<int, 8> &, int);
	void gen_sliding_moves(const std::array<int, 4> &, int);
	void gen_pawn_moves(int);
	void gen_en_passants();
	void gen_castles();
	void generate_moves();
	void update_attack_lists();
	void add_white_pawn_attacks(int from);
	void add_black_pawn_attacks(int from);
	void add_non_sliding_attacks(int from, const std::array<int, 8> &);
	void add_sliding_attacks(int from, const std::array<int, 4> &);

	move_list moves;

	/* Clear the board, reset all flags */
	void clear();

	bool is_king_attacked;

	std::array<attack_list, 6> white_attacks;
	std::array<attack_list, 6> black_attacks;
};

extern std::string starting_FEN;
}
}

#endif // EXAMPLES_PCHESS_POSITION_H
