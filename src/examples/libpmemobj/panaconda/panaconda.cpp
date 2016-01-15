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
 * panaconda.cpp -- example of usage c++ bindings in nvml
 */
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <fstream>
#include <sstream>
#include <ncurses.h>
#include "panaconda.hpp"

#define	LAYOUT_NAME		"pAnaconda"
#define	DEFAULT_DELAY	120000

#define	SNAKE_START_POS_X	5
#define	SNAKE_START_POS_Y	5
#define	SNAKE_START_DIR		(Direction::RIGHT)
#define	SNAKE_STAR_SEG_NO	5

#define	BOARD_STATIC_SIZE_ROW	40
#define	BOARD_STATIC_SIZE_COL	30

#define	PLAYER_POINTS_PER_HIT	10

using namespace nvml::obj;
using namespace std;


//###################################################################//
//				Helper
//###################################################################//
struct ColorPair Helper::getColor(const int aShape)
{
	struct ColorPair res;
	switch(aShape) {
		case SNAKE_SEGMENT:
			res = ColorPair(COLOR_WHITE, COLOR_BLACK);
			break;
		case WALL:
			res = ColorPair(COLOR_BLUE, COLOR_BLUE);
			break;
		case FOOD:
			res = ColorPair(COLOR_RED, COLOR_BLACK);
			break;
		default:
			res = ColorPair(COLOR_BLACK, COLOR_BLACK);
			break;
	}
	return res;
}

//###################################################################//
//				Shape
//###################################################################//
Shape::Shape(int aShape)
{
	int nCurvesSymbol = getSymbol(aShape);
	mVal = COLOR_PAIR(aShape) | nCurvesSymbol;
}

int Shape::getSymbol(int aShape)
{
	int symbol = 0;
	switch(aShape) {
		case SNAKE_SEGMENT:
			symbol = ACS_DIAMOND;
			break;
		case WALL:
			symbol = ACS_BLOCK;
			break;
		case FOOD:
			symbol = ACS_CKBOARD;
			break;

		default:
			symbol = ACS_DIAMOND;
			break;
	}
	return symbol;
}


//###################################################################//
//				Element
//###################################################################//
persistent_ptr<Point> Element::calcNewPosition(const Direction aDir)
{
	persistent_ptr<Point> point = make_persistent<Point>
		(mPoint->mX, mPoint->mY);

	switch(aDir) {
		case Direction::DOWN:
			point->mY = point->mY + 1;
			break;
		case Direction::LEFT:
			point->mX = point->mX - 1;
			break;
		case Direction::RIGHT:
			point->mX = point->mX + 1;
			break;
		case Direction::UP:
			point->mY = point->mY - 1;
			break;
		default:
			break;
	}

	return point;
}

void Element::setPosition(const persistent_ptr<Point> aNewPoint)
{
	persistent_ptr<Point> tempPoint = mPoint;
	mPoint = aNewPoint;
	delete_persistent<Point>(tempPoint);
}

persistent_ptr<Point> Element::getPosition(void)
{
	return mPoint;
}



void Element::print(void)
{
	mvaddch(mPoint->mY, mPoint->mX, mShape->getVal());
}

void Element::printDoubleCol(void)
{
	mvaddch(mPoint->mY, (2 * mPoint->mX), mShape->getVal());
}

void Element::printSingleDoubleCol(void)
{
	mvaddch(mPoint->mY, (2 * mPoint->mX), mShape->getVal());
	mvaddch(mPoint->mY, (2 * mPoint->mX - 1), mShape->getVal());
}

//###################################################################//
//				Snake
//###################################################################//
Snake::Snake()
{
	persistent_ptr<Shape> shape = make_persistent<Shape>(SNAKE_SEGMENT);
	persistent_ptr<Element> element;

	for (unsigned i = 0; i <SNAKE_STAR_SEG_NO; ++i) {
		element = make_persistent<Element>(SNAKE_START_POS_X-i,
				SNAKE_START_POS_Y, shape , SNAKE_START_DIR);
		mSnakeSegments->push_back(element);
	}

	mLastSegPosition = Point();
	mLastSegDir = Direction::RIGHT;
}


Snake::~Snake()
{
	mSnakeSegments->clear();
}


void Snake::move(const Direction aDir)
{
	int snakeSize = mSnakeSegments->size();
	persistent_ptr<Point> newPositionPoint;

	mLastSegPosition = *(mSnakeSegments->get(snakeSize - 1)
			->getPosition().get());
	mLastSegDir = mSnakeSegments->get(snakeSize - 1)->getDirection();

	for (int i = (snakeSize - 1); i >= 0; --i) {
		if (i == 0) {
			newPositionPoint = mSnakeSegments->get(i)->calcNewPosition(aDir);
			mSnakeSegments->get(i)->setDirection(aDir);
		} else {
			newPositionPoint = mSnakeSegments->get(i)->calcNewPosition
				(mSnakeSegments->get(i-1)->getDirection());
			mSnakeSegments->get(i)->setDirection(mSnakeSegments
				->get(i - 1)->getDirection());
		}
		mSnakeSegments->get(i)->setPosition(newPositionPoint);
	}
}


void Snake::print(void)
{
	int i = 0;
	persistent_ptr<Element> segp;

	while ((segp = mSnakeSegments->get(i++)) != nullptr)
		segp->printDoubleCol();
}


void Snake::addSegment(void)
{
	persistent_ptr<Shape> shape = make_persistent<Shape>(SNAKE_SEGMENT);
	persistent_ptr<Element> segp
		= make_persistent<Element>(mLastSegPosition, shape , mLastSegDir);
	mSnakeSegments->push_back(segp);
}

bool Snake::checkPointAgainstSegments(Point aPoint)
{
	int i = 0;
	bool result = false;
	persistent_ptr<Element> segp;

	while ((segp = mSnakeSegments->get(i++)) != nullptr) {
		if (aPoint == *(segp->getPosition().get())) {
			result = true;
			break;
		}
	}
	return result;
}

Point Snake::getHeadPoint(void)
{
	return *(mSnakeSegments->get(0)->getPosition().get());
}


Direction Snake::getDirection(void)
{
	return mSnakeSegments->get(0)->getDirection();
}

Point Snake::getNextPoint(const Direction aDir)
{
	return *(mSnakeSegments->get(0)->calcNewPosition(aDir).get());
}

//###################################################################//
//				Board
//###################################################################//

Board::Board()
{
	persistent_ptr<Shape> shape = make_persistent<Shape>(FOOD);
	persistent_ptr<Element> mFood
			= make_persistent<Element>(0, 0, shape , Direction::UNDEFINED);
	mSnake = make_persistent<Snake>();
	mSizeRow = 20;
	mSizeCol = 20;
}

Board::~Board()
{
	mLayout->clear();
}

void Board::print(const int aScore)
{
	const int offsetY = 2 * mSizeCol + 5;
	const int offsetX = 2;

	int i = 0;
	persistent_ptr<Element> elmp;

	while ((elmp = mLayout->get(i++)) != nullptr)
		elmp->printDoubleCol();

	mSnake->print();
	mFood->printDoubleCol();

	mvprintw((offsetX + 0), offsetY, " ##### pAnaconda ##### ");
	mvprintw((offsetX + 1), offsetY, " #                   # ");
	mvprintw((offsetX + 2), offsetY, " #    q - quit       # ");
	mvprintw((offsetX + 3), offsetY, " #    n - new game   # ");
	mvprintw((offsetX + 4), offsetY, " #                   # ");
	mvprintw((offsetX + 5), offsetY, " ##################### ");

	mvprintw((offsetX + 7), offsetY, " Score: %d ", aScore);
}

void Board::printGameOver(const int aScore)
{
	int x = mSizeCol / 3;
	int y = mSizeRow / 6;
	mvprintw(y + 0, x, "#######   #######   #     #   #######");
	mvprintw(y + 1, x, "#         #     #   ##   ##   #      ");
	mvprintw(y + 2, x, "#   ###   #######   # # # #   ####   ");
	mvprintw(y + 3, x, "#     #   #     #   #  #  #   #      ");
	mvprintw(y + 4, x, "#######   #     #   #     #   #######");

	mvprintw(y + 6, x, "#######   #     #    #######   #######");
	mvprintw(y + 7, x, "#     #   #     #    #         #     #");
	mvprintw(y + 8, x, "#     #    #   #     ####      #######");
	mvprintw(y + 9, x, "#     #     # #      #         #   #  ");
	mvprintw(y + 10, x, "#######      #       #######   #     #");

	mvprintw(y + 12, x, " Last score: %d ", aScore);
	mvprintw(y + 14, x, " q - quit");
	mvprintw(y + 15, x, " n - new game");

}

void Board::creatDynamicLayout(const unsigned aRowNo, char * const aBuffer)
{
	persistent_ptr<Element> element;
	persistent_ptr<Shape> shape
			= make_persistent<Shape>(WALL);

	for (int i = 0; i <mSizeCol; ++i) {
		if (aBuffer[i] == ConfigFileSymbol::SYM_WALL) {
			element = element = make_persistent<Element>(aRowNo, i,
					shape, Direction::UNDEFINED);
			mLayout->push_back(element);
		}
	}
}

void Board::creatStaticLayout(void)
{
	persistent_ptr<Element> element;
	persistent_ptr<Shape> shape = make_persistent<Shape>(WALL);

	mSizeRow = BOARD_STATIC_SIZE_ROW;
	mSizeCol = BOARD_STATIC_SIZE_COL;

	// first and last row
	for (int i = 0; i <mSizeCol; ++i) {
		element = element = make_persistent<Element>(i, 0, shape,
				Direction::UNDEFINED);
		mLayout->push_back(element);
		element = element = make_persistent<Element>(i, (mSizeRow - 1),
				shape, Direction::UNDEFINED);
		mLayout->push_back(element);
	}

	// middle rows
	for (int i = 1; i <mSizeRow; ++i) {
		element = element = make_persistent<Element>(0, i, shape,
				Direction::UNDEFINED);
		mLayout->push_back(element);
		element = element = make_persistent<Element>((mSizeCol - 1), i,
				shape, Direction::UNDEFINED);
		mLayout->push_back(element);
	}
}

bool Board::isSnakeCollision(Point aPoint)
{
	return mSnake->checkPointAgainstSegments(aPoint);
}


bool Board::isWallCollision(Point aPoint)
{
	int i = 0;
	bool result = false;
	persistent_ptr<Element> wallp;

	while ((wallp = mLayout->get(i++)) != nullptr) {
		if (aPoint == *(wallp->getPosition().get())) {
			result = true;
			break;
		}
	}
	return result;
}

bool Board::isCollision(Point aPoint)
{
	return isSnakeCollision(aPoint) || isWallCollision(aPoint);
}

bool Board::isSnakeHeadFoodHit(void)
{
	bool result = false;
	Point headPoint = mSnake->getHeadPoint();

	if (headPoint == *(mFood->getPosition().get())) {
		result = true;
	}
	return result;
}

void Board::setNewFood(const Point aPoint)
{
	persistent_ptr<Shape> shape = make_persistent<Shape>(FOOD);
	delete_persistent<Element>(mFood);
	mFood = make_persistent<Element>(aPoint, shape , Direction::UNDEFINED);
}

void Board::createNewFood(void)
{
	const int maxRepeat = 50;
	int count = 0;
	int randRow = 0;
	int randCol = 0;

	while (count <maxRepeat) {
		randRow = 1 + rand() % ((getSizeRow() / 2) - 1);
		randCol = 1 + rand() % (getSizeCol() - 1);

		Point foodPoint(randRow, randCol);
		if (!isSnakeCollision(foodPoint)) {
			setNewFood(foodPoint);
			break;
		}
		count++;
	}
}

SnakeEvent Board::moveSnake(const Direction aDir)
{
	SnakeEvent event = SnakeEvent::EV_OK;
	Point nextPt = mSnake->getNextPoint(aDir);

	if (isCollision(nextPt)) {
		event = SnakeEvent::EV_COLLISION;
	} else {
		mSnake->move(aDir);
	}

	return event;
}


//###################################################################//
//				GameState
//###################################################################//
void GameState::init(void)
{
	mBoard = make_persistent<Board>();
	mPlayer = make_persistent<Player>();
}


void GameState::cleanPool(void)
{
	delete_persistent<Board>(mBoard);
	mBoard= nullptr;

	delete_persistent<Player>(mPlayer);
	mPlayer = nullptr;
}

//###################################################################//
//				Player
//###################################################################//
void Player::updateScore(void)
{
	mScore = mScore + PLAYER_POINTS_PER_HIT;
}

//###################################################################//
//				Game
//###################################################################//
Game::Game(const string aName)
{
	pool<GameState> pop;

	initscr();
	start_color();
	nodelay(stdscr, true);
	curs_set(0);
	keypad(stdscr, true);

	if (pop.exists(aName, LAYOUT_NAME)) {
		pop.open(aName, LAYOUT_NAME);
	} else {
		pop.create(aName, LAYOUT_NAME, PMEMOBJ_MIN_POOL*10, 0666);
	}
	mGameState = pop;
	mDirectionKey = Direction::UNDEFINED;
	mLastKey = KEY_CLEAR;
	mDelay = DEFAULT_DELAY;

	initColors();

	srand(time(0));
}


void Game::initColors(void)
{
	struct ColorPair colorPair = Helper::getColor(SNAKE_SEGMENT);
	init_pair(SNAKE_SEGMENT, colorPair.colorFg, colorPair.colorBg);

	colorPair = Helper::getColor(WALL);
	init_pair(WALL, colorPair.colorFg, colorPair.colorBg);

	colorPair = Helper::getColor(FOOD);
	init_pair(FOOD, colorPair.colorFg, colorPair.colorBg);
}


void Game::init(void)
{
	auto r = mGameState.get_root();

	if (r->getBoard() == nullptr) {
		try {
			transaction::exec_tx([&]() {
				r->init();
				r->getBoard()->creatStaticLayout();
				r->getBoard()->createNewFood();
			});
		}
		catch (transaction_error &err) {
			cout <<err.what() <<endl;
		}
	}
	mDirectionKey = r->getBoard()->getSnakeDir();
}


void Game::processStep(void)
{
	SnakeEvent retEvent = EV_OK;
	auto r = mGameState.get_root();
	try {
		mGameState.exec_tx([&]() {
			retEvent = r->getBoard()->moveSnake(mDirectionKey);
			if (EV_COLLISION == retEvent) {
				r->getPlayer()->setState(State::STATE_GAMEOVER);
				return;
			} else {
				if (r->getBoard()->isSnakeHeadFoodHit()) {
					r->getBoard()->createNewFood();
					r->getBoard()->addSnakeSegment();
					r->getPlayer()->updateScore();
				}
			}
		});
	}
	catch (transaction_error &err) {
		cout <<err.what() <<endl;
	}

	r->getBoard()->print(r->getPlayer()->getScore());
}

inline bool Game::isStopped(void)
{
	if (Action::ACTION_QUIT == mLastKey)
		return true;
	else
		return false;
}

void Game::setDirectionKey(void)
{
	switch (mLastKey) {
		case KEY_LEFT:
			if (Direction::RIGHT != mDirectionKey)
				mDirectionKey = Direction::LEFT;
			break;
		case KEY_RIGHT:
			if (Direction::LEFT != mDirectionKey)
				mDirectionKey = Direction::RIGHT;
			break;
		case KEY_UP:
			if (Direction::DOWN != mDirectionKey)
				mDirectionKey = Direction::UP;
			break;
		case KEY_DOWN:
			if (Direction::UP != mDirectionKey)
				mDirectionKey = Direction::DOWN;
			break;
		default:
			break;
	}
}

void Game::processKey(const int aLastKey)
{
	mLastKey = aLastKey;
	setDirectionKey();

	if (Action::ACTION_NEW_GAME == aLastKey) {
		cleanPool();
		init();
	}

}

void Game::cleanPool(void)
{
	auto r = mGameState.get_root();

	try {
		mGameState.exec_tx([&]() {
			r->cleanPool();
		});
	} catch (transaction_error &err) {
		cout <<err.what() <<endl;
	}
}

void Game::delay(void)
{
	Helper::sleep(mDelay);
}

void Game::clear(void)
{
	erase();
}

void Game::gameOver(void)
{
	auto r = mGameState.get_root();
	r->getBoard()->printGameOver(r->getPlayer()->getScore());
}


bool Game::isGameOver(void)
{
	auto r = mGameState.get_root();
	return (r->getPlayer()->getState() == State::STATE_GAMEOVER);
}

void Game::parseConfCreatDynamicLayout(void)
{
	unsigned buffSize = 8;
	char buffer[buffSize];
	int dimension[2];
	unsigned rowNo = 0;
	unsigned colNo = 0;
	ifstream cfgFile;

	cfgFile.open("conf.cfg");
	cfgFile.getline(buffer, buffSize);
	auto r = mGameState.get_root();

	// read x and y
	string  str(buffer);
	stringstream stringBuff(str);

	if (!stringBuff) {
		cout <<"Config file error" <<endl;
	} else {
		for (int i = 0; i <2; ++i)
			stringBuff>> dimension[i];
		rowNo = dimension[0];
		colNo = dimension[1];

		try {
			mGameState.exec_tx([&]() {
				r->getBoard()->setSizeRow(rowNo);
				r->getBoard()->setSizeCol(colNo);
			});
		} catch (transaction_error &err) {
			cout <<err.what() <<endl;
		}

		// read whole board map
		char* rowBuffer = new char[colNo];
		cfgFile.getline(rowBuffer, colNo);
		for (unsigned i = 0; i <rowNo; ++i) {
			cfgFile.getline(rowBuffer, colNo);
			try {
				mGameState.exec_tx([&]() {
					r->getBoard()->creatDynamicLayout(i, rowBuffer);
				});
			} catch (transaction_error &err) {
				cout <<err.what() <<endl;
			}
		}
	}
	cfgFile.close();
}

//###################################################################//
//				main
//###################################################################//
int main(int argc, char *argv[])
{
	int input;
	Game* game;
	if (argc != 2)
		return 0;

	string name = argv[1];
	game = new Game(name);
	game->init();

	while (!game->isStopped()) {
		input = getch();
		game->processKey(input);
		if (game->isGameOver()) {
			game->gameOver();
		} else {
			game->delay();
			game->clear();
			game->processStep();
		}
	}

	game->closePool();
	endwin();
	return 0;
}
