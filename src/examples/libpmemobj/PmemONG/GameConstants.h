/*
 * GameConstants.h
 *
 *  Created on: 12 lip 2017
 *      Author: huber
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


#endif /* GAMECONSTANTS_H_ */
