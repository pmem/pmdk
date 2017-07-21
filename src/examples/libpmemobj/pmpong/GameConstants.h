/*
* Copyright 2017, Intel Corporation
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

#ifndef GAMECONSTANTS_H_
#define GAMECONSTANTS_H_

#include <SFML/Graphics.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/transaction.hpp>
#include <libpmemobj++/pool.hpp>
#include <random>

#define PADDLE_VELOCITY_PLAYER 4
#define PADDLE_VELOCITY_COMPUTER 20
#define WINDOW_HEIGHT 500
#define WINDOW_WIDTH 700
#define BALL_VELOCITY_INCREMENTING 0.2
#define FRAMERATE_LIMIT 70
#define PADDLE_HEIGHT 100
#define PADDLE_WIDTH 12
#define BALL_SIZE 7
#define MENU_ITEMS 4
#define POINTS_TO_WIN 10
#define BALL_PLAYERS_SPEED 4.0
#define BALL_COMUTER_SPEED 11.0
#define VERTICAL_LINE_OFFSET 15
#define HORIZONAL_LINE_OFFSET 30
#define LINE_THICKNESS 3
#define SCORE_VIEW_OFFSET 20
#define GAME_NAME "PmemONG"
#define GAMEVIEW_SCORE_FONTSIZE 20
#define MENUVIEW_ITEMS_FONTSIZE 30
#define GAMEOVER_FONTSIZE 45
#define MENUITEM_OFFSET 100
#define GAMOVERVIEW_OFFSET 50
#define LAYOUT_NAME "DEFAULT_LAYOUT_NAME"
#define DEFAULT_POOLFILE_NAME "DEFAULT_FILENAME"

#ifndef _WIN32
#define FONT_PATH "NotoSansCJK-Regular.ttc"
#else
#define FONT_PATH "C:/Windows/Fonts/Arial.ttf"
#endif


#endif /* GAMECONSTANTS_H_ */
