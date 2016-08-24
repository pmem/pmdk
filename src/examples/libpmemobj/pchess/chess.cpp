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
 * chess.cpp - some general chess related utility functions
 */

#include "chess.hpp"
#include "position.hpp"

#include <cctype>
#include <cstring>

using std::string;

namespace examples
{
namespace pchess
{

bool
is_char_piece(char c) noexcept
{
	return std::strchr("RNBQKPrnbqkp", c) != nullptr;
}

square
char_to_square(char c) noexcept
{
	square sq;

	sq.is_empty = false;
	sq.is_valid = true;

	switch (std::toupper(c)) {
		case 'R':
			sq.piece_type = rook;
			break;
		case 'B':
			sq.piece_type = bishop;
			break;
		case 'N':
			sq.piece_type = knight;
			break;
		case 'Q':
			sq.piece_type = queen;
			break;
		case 'K':
			sq.piece_type = king;
			break;
		case 'P':
			sq.piece_type = pawn;
			break;
	}

	if (std::isupper(c))
		sq.piece_side = white;
	else
		sq.piece_side = black;

	return sq;
}

bool
is_rank_char(char c) noexcept
{
	return c >= '1' and c <= '8';
}

bool
is_file_char(char c) noexcept
{
	return (c >= 'a' and c <= 'h') or (c >= 'A' and c <= 'H');
}

int
char_to_rank(char c) noexcept
{
	return 9 - (c - '1');
}

int
char_to_file(char c) noexcept
{
	return 8 - (c - 'a');
}

int
parse_coordinates(string::const_iterator c)
{
	int rank;
	int file;

	if (not is_file_char(*c))
		throw;

	rank = char_to_file(*c);

	c++;

	if (not is_rank_char(*c))
		throw;

	file = char_to_rank(*c);

	return index_at(file, rank);
}

char
rank_to_char(int rank) noexcept
{
	return (char)('1' + (rank_1 - rank));
}

char
file_to_char(int file) noexcept
{
	return (char)('a' + (file_a - file));
}

string
print_coordinates(int index) noexcept
{
	string result;

	result += file_to_char(file_at(index));
	result += rank_to_char(rank_at(index));

	return result;
}

char
square_to_char(square sq) noexcept
{
	char c;

	switch (sq.piece_type) {
		case pawn:
			c = 'P';
			break;
		case bishop:
			c = 'B';
			break;
		case rook:
			c = 'R';
			break;
		case knight:
			c = 'N';
			break;
		case queen:
			c = 'Q';
			break;
		case king:
			c = 'K';
			break;
		default:
			__builtin_unreachable();
	}

	if (sq.piece_side == black)
		c = std::tolower(c);

	return c;
}
}
}
