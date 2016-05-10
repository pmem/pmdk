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
 * panaconda.hpp -- example of usage c++ bindings in nvml
 */

#ifndef PANACONDA_HPP
#define PANACONDA_HPP

#include <libpmemobj/make_persistent_array.hpp>
#include <libpmemobj/p.hpp>
#include <libpmemobj/pool.hpp>
#include <libpmemobj/transaction.hpp>

#include "list.hpp"

class Element;

enum direction { UNDEFINED, DOWN, RIGHT, UP, LEFT };
enum object_type { SNAKE_SEGMENT, WALL, FOOD };
enum config_file_symbol { SYM_NOTHING = '0', SYM_WALL = '1' };
enum state { STATE_NEW, STATE_PLAY, STATE_GAMEOVER };

enum snake_event {
	EV_OK,
	EV_COLLISION
};

enum action {
	ACTION_NEW_GAME = 'n',
	ACTION_QUIT = 'q'
};

typedef nvml::obj::persistent_ptr<examples::list<Element>> element_list;

struct ColorPair {
	ColorPair();
	ColorPair(const int aColFg, const int aColBg);
	int colorBg;
	int colorFg;
};

struct Params {
	bool use_maze;
	std::string name;
	std::string maze_path;
};

class Helper {
public:
	static ColorPair getColor(const object_type aShape);
	static int parseParams(int argc, char *argv[], struct Params *params);
	static inline void sleep(int aTime);
	static inline void print_usage(std::string &name);
};

class Point {
public:
	nvml::obj::p<int> mX;
	nvml::obj::p<int> mY;

	Point();
	Point(int aX, int aY);
	friend bool operator==(Point &aPoint1, Point &aPoint2);
};

bool operator==(Point &aPoint1, Point &aPoint2);

class Shape {
public:
	Shape() = default;
	Shape(int aShape);
	int getVal();

private:
	nvml::obj::p<int> mVal;
	int getSymbol(int aShape);
};

class Element {
public:
	Element();
	Element(int aX, int aY, nvml::obj::persistent_ptr<Shape> aShape,
		direction aDir);
	Element(Point aPoint, nvml::obj::persistent_ptr<Shape> aShape,
		direction aDir);
	Element(const Element &aElement);
	~Element();

	nvml::obj::persistent_ptr<Point> calcNewPosition(const direction aDir);
	void print(void);
	void printDoubleCol(void);
	void printSingleDoubleCol(void);
	nvml::obj::persistent_ptr<Point> getPosition(void);
	void setPosition(const nvml::obj::persistent_ptr<Point> aNewPoint);
	direction getDirection(void);
	void setDirection(const direction aDir);

private:
	nvml::obj::persistent_ptr<Point> mPoint;
	nvml::obj::persistent_ptr<Shape> mShape;
	nvml::obj::p<direction> mDirection;
};

class Snake {
public:
	Snake();
	~Snake();

	void move(const direction aDir);
	void print(void);
	void addSegment(void);
	bool checkPointAgainstSegments(Point aPoint);
	Point getHeadPoint(void);
	direction getDirection(void);
	Point getNextPoint(const direction aDir);

private:
	element_list mSnakeSegments;
	nvml::obj::p<Point> mLastSegPosition;
	nvml::obj::p<direction> mLastSegDir;
};

class Board {
public:
	Board();
	~Board();
	void print(const int aScore);
	void printGameOver(const int aScore);
	unsigned getSizeRow(void);
	void setSizeRow(const unsigned aSizeRow);
	unsigned getSizeCol(void);
	void setSizeCol(const unsigned aSizeCol);
	int creatDynamicLayout(const unsigned aRowNo, char *const aBuffer);
	int creatStaticLayout(void);
	bool isSnakeHeadFoodHit(void);
	void createNewFood(void);
	bool isCollision(Point aPoint);
	snake_event moveSnake(const direction aDir);
	direction getSnakeDir(void);
	void addSnakeSegment(void);

private:
	nvml::obj::persistent_ptr<Snake> mSnake;
	nvml::obj::persistent_ptr<Element> mFood;
	element_list mLayout;

	nvml::obj::p<unsigned> mSizeRow;
	nvml::obj::p<unsigned> mSizeCol;

	void setNewFood(const Point aPoint);
	bool isSnakeCollision(Point aPoint);
	bool isWallCollision(Point aPoint);
};

class Player {
public:
	Player();
	~Player();
	int
	getScore(void);
	void updateScore(void);
	state getState(void);
	void setState(const state aState);

private:
	nvml::obj::p<int> mScore;
	nvml::obj::p<state> mState;
};

class GameState {
public:
	GameState();
	~GameState();
	nvml::obj::persistent_ptr<Board> getBoard();
	nvml::obj::persistent_ptr<Player> getPlayer();
	void init(void);
	void cleanPool(void);

private:
	nvml::obj::persistent_ptr<Board> mBoard;
	nvml::obj::persistent_ptr<Player> mPlayer;
};

class Game {
public:
	Game(struct Params *params);
	~Game();
	int init(void);
	void initColors(void);
	void processStep(void);
	int processKey(const int aLastKey);
	inline bool isStopped(void);
	void delay(void);
	void clearScreen(void);
	bool isGameOver(void);
	void gameOver(void);
	void clearProg(void);

private:
	nvml::obj::pool<GameState> mGameState;
	int mLastKey;
	int mDelay;
	struct Params *mParams;
	direction mDirectionKey;

	void cleanPool(void);
	void setDirectionKey(void);
	int parseConfCreateDynamicLayout(void);
};

#endif /* PANACONDA_HPP */
