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
 * chess.hpp - Some basic chess related constants, types.
 */

#ifndef EXAMPLES_PCHESS_CHESS_H
#define EXAMPLES_PCHESS_CHESS_H

#include <array>
#include <string>

namespace examples
{
namespace pchess
{

enum piece { PAWN, ROOK, BISHOP, KNIGHT, QUEEN, KING };

enum side { WHITE, BLACK };

constexpr side
opponent(side who)
{
	return (who == WHITE) ? BLACK : WHITE;
}

struct square {
	bool is_valid;
	bool is_empty;
	piece piece_type;
	side piece_side;

	bool
	is_valid_target(side for_whom) const
	{
		return is_valid && (is_empty || piece_side != for_whom);
	}

	bool
	operator==(const square &other) const
	{
		return is_valid == other.is_valid and
			is_empty == other.is_empty and
			piece_type == other.piece_type and
			piece_side == other.piece_side;
	}
};

constexpr square white_king = {true, false, KING, WHITE};
constexpr square black_king = {true, false, KING, BLACK};
constexpr square white_pawn = {true, false, PAWN, WHITE};
constexpr square black_pawn = {true, false, PAWN, BLACK};

enum class move_type {
	GENERAL,
	PAWN_DOUBLE_PUSH,
	CASTLE,
	EN_PASSANT,
	PROMOTE_QUEEN,
	PROMOTE_KNIGHT,
	PROMOTE_BISHOP,
	PROMOTE_ROOK
};

struct move {
	int from;
	int to;
	move_type type = move_type::GENERAL;

	bool
	is_null() const
	{
		return from == 0 and to == 0;
	}

	constexpr move(int f, int t, move_type ty = move_type::GENERAL)
	    : from(f), to(t), type(ty)
	{
	}

	move()
	{
	}
};

constexpr move null_move = {0, 0};

enum { east = -1, west = 1, north = -10, south = 10 };

constexpr int
rank_at(int index)
{
	return index / 10;
}

constexpr int
file_at(int index)
{
	return index % 10;
}

constexpr int
index_at(int file, int rank)
{
	return rank * 10 + file;
}

enum { RANK_1 = 9,
       RANK_2 = 8,
       RANK_3 = 7,
       RANK_4 = 6,
       RANK_5 = 5,
       RANK_6 = 4,
       RANK_7 = 3,
       RANK_8 = 2,

       FILE_A = 8,
       FILE_B = 7,
       FILE_C = 6,
       FILE_D = 5,
       FILE_E = 4,
       FILE_F = 3,
       FILE_G = 2,
       FILE_H = 1,

       SQ_H8 = 21,
       SQ_G8 = 22,
       SQ_F8 = 23,
       SQ_E8 = 24,
       SQ_D8 = 25,
       SQ_C8 = 26,
       SQ_B8 = 27,
       SQ_A8 = 28,

       SQ_H7 = 31,
       SQ_G7 = 32,
       SQ_F7 = 33,
       SQ_E7 = 34,
       SQ_D7 = 35,
       SQ_C7 = 36,
       SQ_B7 = 37,
       SQ_A7 = 38,

       SQ_H6 = 41,
       SQ_G6 = 42,
       SQ_F6 = 43,
       SQ_E6 = 44,
       SQ_D6 = 45,
       SQ_C6 = 46,
       SQ_B6 = 47,
       SQ_A6 = 48,

       SQ_H5 = 51,
       SQ_G5 = 52,
       SQ_F5 = 53,
       SQ_E5 = 54,
       SQ_D5 = 55,
       SQ_C5 = 56,
       SQ_B5 = 57,
       SQ_A5 = 58,

       SQ_H4 = 61,
       SQ_G4 = 62,
       SQ_F4 = 63,
       SQ_E4 = 64,
       SQ_D4 = 65,
       SQ_C4 = 66,
       SQ_B4 = 67,
       SQ_A4 = 68,

       SQ_H3 = 71,
       SQ_G3 = 72,
       SQ_F3 = 73,
       SQ_E3 = 74,
       SQ_D3 = 75,
       SQ_C3 = 76,
       SQ_B3 = 77,
       SQ_A3 = 78,

       SQ_H2 = 81,
       SQ_G2 = 82,
       SQ_F2 = 83,
       SQ_E2 = 84,
       SQ_D2 = 85,
       SQ_C2 = 86,
       SQ_B2 = 87,
       SQ_A2 = 88,

       SQ_H1 = 91,
       SQ_G1 = 92,
       SQ_F1 = 93,
       SQ_E1 = 94,
       SQ_D1 = 95,
       SQ_C1 = 96,
       SQ_B1 = 97,
       SQ_A1 = 98

};

constexpr bool
is_above_rank_8(int index)
{
	return index < 20;
}

constexpr bool
is_below_rank_1(int index)
{
	return index >= 100;
}

constexpr bool
is_on_west_edge(int index)
{
	return index % 10 == 9;
}

constexpr bool
is_on_east_edge(int index)
{
	return index % 10 == 0;
}

constexpr bool
is_on_edge(int index)
{
	return is_above_rank_8(index) or is_below_rank_1(index) or
		is_on_west_edge(index) or is_on_east_edge(index);
}

constexpr int
south_of(int index)
{
	return index + 10;
}

constexpr int
north_of(int index)
{
	return index - 10;
}

constexpr int
west_of(int index, int delta = 1)
{
	return index + delta;
}

constexpr int
east_of(int index, int delta = 1)
{
	return index - delta;
}

bool is_char_piece(char) noexcept;
square char_to_square(char) noexcept;
char square_to_char(square) noexcept;
bool is_rank_char(char) noexcept;
bool is_file_char(char) noexcept;
int char_to_rank(char) noexcept;
int char_to_file(char) noexcept;
char rank_to_char(int) noexcept;
char file_to_char(int) noexcept;
int parse_coordinates(std::string::const_iterator);
std::string print_coordinates(int) noexcept;

struct move_list {
	unsigned count = 0;
	std::array<move, 256> items;

	void
	push_back(move m)
	{
		items[count++] = m;
	}

	move operator[](unsigned i) const
	{
		return items[i];
	}

	move &operator[](unsigned i)
	{
		return items[i];
	}

	const move *
	begin() const
	{
		return &items[0];
	}

	const move *
	end() const
	{
		return &items[count];
	}

	size_t
	size() const
	{
		return count;
	}
};
}
}

#endif // EXAMPLES_PCHESS_CHESS_H
