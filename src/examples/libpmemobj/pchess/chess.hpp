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

enum piece { pawn, rook, bishop, knight, queen, king };

enum side { white, black };

constexpr side
opponent(side who)
{
	return (who == white) ? black : white;
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

constexpr square white_king = {true, false, king, white};
constexpr square black_king = {true, false, king, black};
constexpr square white_pawn = {true, false, pawn, white};
constexpr square black_pawn = {true, false, pawn, black};

enum class move_type {
	general,
	pawn_double_push,
	castle,
	en_passant,
	promote_queen,
	promote_knight,
	promote_bishop,
	promote_rook
};

struct move {
	int from;
	int to;
	move_type type = move_type::general;

	bool
	is_null() const
	{
		return from == 0 and to == 0;
	}

	constexpr move(int f, int t, move_type ty = move_type::general)
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

enum { rank_1 = 9,
       rank_2 = 8,
       rank_3 = 7,
       rank_4 = 6,
       rank_5 = 5,
       rank_6 = 4,
       rank_7 = 3,
       rank_8 = 2,

       file_a = 8,
       file_b = 7,
       file_c = 6,
       file_d = 5,
       file_e = 4,
       file_f = 3,
       file_g = 2,
       file_h = 1,

       sq_h8 = 21,
       sq_g8 = 22,
       sq_f8 = 23,
       sq_e8 = 24,
       sq_d8 = 25,
       sq_c8 = 26,
       sq_b8 = 27,
       sq_a8 = 28,

       sq_h7 = 31,
       sq_g7 = 32,
       sq_f7 = 33,
       sq_e7 = 34,
       sq_d7 = 35,
       sq_c7 = 36,
       sq_b7 = 37,
       sq_a7 = 38,

       sq_h6 = 41,
       sq_g6 = 42,
       sq_f6 = 43,
       sq_e6 = 44,
       sq_d6 = 45,
       sq_c6 = 46,
       sq_b6 = 47,
       sq_a6 = 48,

       sq_h5 = 51,
       sq_g5 = 52,
       sq_f5 = 53,
       sq_e5 = 54,
       sq_d5 = 55,
       sq_c5 = 56,
       sq_b5 = 57,
       sq_a5 = 58,

       sq_h4 = 61,
       sq_g4 = 62,
       sq_f4 = 63,
       sq_e4 = 64,
       sq_d4 = 65,
       sq_c4 = 66,
       sq_b4 = 67,
       sq_a4 = 68,

       sq_h3 = 71,
       sq_g3 = 72,
       sq_f3 = 73,
       sq_e3 = 74,
       sq_d3 = 75,
       sq_c3 = 76,
       sq_b3 = 77,
       sq_a3 = 78,

       sq_h2 = 81,
       sq_g2 = 82,
       sq_f2 = 83,
       sq_e2 = 84,
       sq_d2 = 85,
       sq_c2 = 86,
       sq_b2 = 87,
       sq_a2 = 88,

       sq_h1 = 91,
       sq_g1 = 92,
       sq_f1 = 93,
       sq_e1 = 94,
       sq_d1 = 95,
       sq_c1 = 96,
       sq_b1 = 97,
       sq_a1 = 98

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
