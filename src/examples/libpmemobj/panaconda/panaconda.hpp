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

#ifndef PANACONDA_HPP_
#define PANACONDA_HPP_

#include <libpmemobj/make_persistent_array.hpp>
#include <libpmemobj/p.hpp>
#include <libpmemobj/pool.hpp>
#include <libpmemobj/transaction.hpp>

#include "list.hpp"

//###################################################################//
//				Forward decl
//###################################################################//
class Element;

//###################################################################//
//				Types
//###################################################################//
enum Direction { UNDEFINED, DOWN, RIGHT, UP, LEFT };

enum ObjectType { SNAKE_SEGMENT, WALL, FOOD } ObjectType_t;

enum ConfigFileSymbol { SYM_NOTHING = '0', SYM_WALL = '1' };

enum SnakeEvent {
	EV_OK,
	EV_COLLISION

};

enum State { STATE_NEW, STATE_PLAY, STATE_GAMEOVER };

enum Action {
	ACTION_NEW_GAME = 'n',
	ACTION_QUIT = 'q'

};

typedef nvml::obj::persistent_ptr<examples::list<Element>> ElementList;

//###################################################################//
//				Classes
//###################################################################//

struct ColorPair {
	ColorPair() : colorBg(COLOR_BLACK), colorFg(COLOR_BLACK)
	{
	}
	ColorPair(const int aColFg, const int aColBg)
	    : colorBg(aColBg), colorFg(aColFg)
	{
	}
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
	static ColorPair getColor(const int aShape);
	static int parseParams(int argc, char *argv[], struct Params *params);

	static inline void
	sleep(int aTime)
	{
		clock_t currTime = clock();
		while (clock() < (currTime + aTime)) {
		}
	}

	static inline void
	print_usage(std::string &name)
	{
		std::cout << "Usage: " << name
			  << " [-m <maze_path>] <pool_name>\n";
	}
};

class Point {
public:
	nvml::obj::p<int> mX;
	nvml::obj::p<int> mY;

	Point() : mX(0), mY(0)
	{
	}
	Point(int aX, int aY) : mX(aX), mY(aY)
	{
	}
	friend bool operator==(Point &aPoint1, Point &aPoint2);
};

bool
operator==(Point &aPoint1, Point &aPoint2)
{
	return aPoint1.mX == aPoint2.mX && aPoint1.mY == aPoint2.mY;
}

class Shape {
public:
	Shape() = default;
	Shape(int aShape);
	int
	getVal()
	{
		return mVal;
	}

private:
	nvml::obj::p<int> mVal;
	int getSymbol(int aShape);
};

class Element {
public:
	Element()
	    : mPoint(nvml::obj::make_persistent<Point>(0, 0)),
	      mShape(nvml::obj::make_persistent<Shape>(SNAKE_SEGMENT)),
	      mDirection(Direction::LEFT)
	{
	}
	Element(int aX, int aY, nvml::obj::persistent_ptr<Shape> aShape,
		Direction aDir)
	    : mPoint(nvml::obj::make_persistent<Point>(aX, aY)),
	      mShape(aShape),
	      mDirection(aDir)
	{
	}
	Element(Point aPoint, nvml::obj::persistent_ptr<Shape> aShape,
		Direction aDir)
	    : mPoint(nvml::obj::make_persistent<Point>(aPoint.mX, aPoint.mY)),
	      mShape(aShape),
	      mDirection(aDir)
	{
	}
	Element(const Element &aElement)
	{
		mPoint = nvml::obj::make_persistent<Point>(aElement.mPoint->mX,
							   aElement.mPoint->mY);
		mShape = nvml::obj::make_persistent<Shape>(
			aElement.mShape->getVal());
	}

	~Element()
	{
		nvml::obj::delete_persistent<Point>(mPoint);
		mPoint = nullptr;
		nvml::obj::delete_persistent<Shape>(mShape);
		mShape = nullptr;
	}

	nvml::obj::persistent_ptr<Point> calcNewPosition(const Direction aDir);
	void print(void);
	void printDoubleCol(void);
	void printSingleDoubleCol(void);
	nvml::obj::persistent_ptr<Point> getPosition(void);
	void setPosition(const nvml::obj::persistent_ptr<Point> aNewPoint);
	Direction
	getDirection(void)
	{
		return mDirection;
	}
	void
	setDirection(const Direction aDir)
	{
		mDirection = aDir;
	}

private:
	nvml::obj::persistent_ptr<Point> mPoint;
	nvml::obj::persistent_ptr<Shape> mShape;
	nvml::obj::p<Direction> mDirection;
};

class Snake {
public:
	Snake();
	~Snake();

	void move(const Direction aDir);
	void print(void);
	void addSegment(void);
	bool checkPointAgainstSegments(Point aPoint);
	Point getHeadPoint(void);
	Direction getDirection(void);
	Point getNextPoint(const Direction aDir);

private:
	ElementList mSnakeSegments;
	nvml::obj::p<Point> mLastSegPosition;
	nvml::obj::p<Direction> mLastSegDir;
};

class Board {
public:
	Board();
	~Board();
	void print(const int aScore);
	void printGameOver(const int aScore);
	unsigned
	getSizeRow(void)
	{
		return mSizeRow;
	}
	void
	setSizeRow(const unsigned aSizeRow)
	{
		mSizeRow = aSizeRow;
	}
	unsigned
	getSizeCol(void)
	{
		return mSizeCol;
	}
	void
	setSizeCol(const unsigned aSizeCol)
	{
		mSizeCol = aSizeCol;
	}
	int creatDynamicLayout(const unsigned aRowNo, char *const aBuffer);
	int creatStaticLayout(void);
	bool isSnakeHeadFoodHit(void);
	void createNewFood(void);
	bool isCollision(Point aPoint);
	SnakeEvent moveSnake(const Direction aDir);
	Direction
	getSnakeDir(void)
	{
		return mSnake->getDirection();
	}
	void
	addSnakeSegment(void)
	{
		mSnake->addSegment();
	}

private:
	nvml::obj::persistent_ptr<Snake> mSnake;
	nvml::obj::persistent_ptr<Element> mFood;
	ElementList mLayout;

	nvml::obj::p<unsigned> mSizeRow;
	nvml::obj::p<unsigned> mSizeCol;

	void setNewFood(const Point aPoint);
	bool isSnakeCollision(Point aPoint);
	bool isWallCollision(Point aPoint);
};

class Player {
public:
	Player() : mScore(0), mState(STATE_PLAY)
	{
	}
	~Player()
	{
	}
	int
	getScore(void)
	{
		return mScore;
	}
	void updateScore(void);
	State
	getState(void)
	{
		return mState;
	}
	void
	setState(const State aState)
	{
		mState = aState;
	}

private:
	nvml::obj::p<int> mScore;
	nvml::obj::p<State> mState;
};

class GameState {
public:
	GameState()
	{
	}
	~GameState()
	{
	}
	nvml::obj::persistent_ptr<Board>
	getBoard()
	{
		return mBoard;
	}
	nvml::obj::persistent_ptr<Player>
	getPlayer()
	{
		return mPlayer;
	}
	void init(void);
	void cleanPool(void);

private:
	nvml::obj::persistent_ptr<Board> mBoard;
	nvml::obj::persistent_ptr<Player> mPlayer;
};

class Game {
public:
	Game(struct Params *params);
	~Game()
	{
	}
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
	Direction mDirectionKey;

	void cleanPool(void);
	void setDirectionKey(void);
	int parseConfCreateDynamicLayout(void);
};

#endif /* PANACONDA_HPP_ */
