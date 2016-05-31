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
 * pman.cpp -- example of usage c++ bindings in nvml
 */

#include "list.hpp"
#include <fstream>
#include <iostream>
#include <libpmemobj/make_persistent_array.hpp>
#include <libpmemobj/pool.hpp>
#include <libpmemobj/transaction.hpp>
#include <ncurses.h>

#define LAYOUT_NAME "pman"
#define SIZE 40
#define MAX_SIZE 38
#define MAX_BOMBS 5
#define KEY_SPACE 32
#define RAND_FIELD() (rand() % (SIZE - 2) + 1)
#define EXPLOSION_TIME 20
#define EXPLOSION_COUNTER 80
#define SLEEP_TIME (2 * CLOCKS_PER_SEC)
#define GAME_DELAY 40000
#define SLEEP(t)                                                               \
	do {                                                                   \
		struct timespec req = {0, t * 1000};                           \
		while (nanosleep(&req, &req) == -1 && errno == EINTR)          \
			;                                                      \
	} while (0)

using nvml::obj::p;
using nvml::obj::persistent_ptr;
using nvml::obj::pool;
using nvml::obj::pool_base;
using nvml::obj::make_persistent;
using nvml::obj::delete_persistent;
using nvml::obj::transaction;
using nvml::transaction_error;

namespace examples
{

class state;

} /* namespace examples */

namespace
{

pool<examples::state> pop;
}

namespace examples
{

enum position {
	UP_LEFT,
	UP_RIGHT,
	DOWN_LEFT,
	DOWN_RIGHT,
	POS_MIDDLE,
	POS_MAX,
};

enum direction {
	DOWN,
	RIGHT,
	UP,
	LEFT,
	STOP,
};

enum field {
	FREE,
	FOOD,
	WALL,
	PLAYER,
	ALIEN,
	EXPLOSION,
	BONUS,
	LIFE,
	BOMB,
};

class point {
public:
	point(){};
	point(int xf, int yf);
	point(position cor);
	void move_back();
	void move_home();
	/* x component of object's position */
	p<int> x;
	/* y component of object's position */
	p<int> y;
	/* x component of object's  previous position */
	p<int> prev_x;
	/* y component of object's  previous position */
	p<int> prev_y;
	/* type of field of object */
	p<field> cur_field;
	/* type of field where object stood before */
	p<field> prev_field;

protected:
	void move();
	/* direction in which object is moving */
	p<direction> dir;

private:
	/* starting position of the object */
	p<position> home;
};

class bomb : public point {
public:
	bomb(int xf, int yf);
	void progress();
	void explosion();
	void print_time();
	/* flag determining is bomb exploded */
	p<bool> exploded;
	/* flag determining is bomb used */
	p<bool> used;

private:
	/* counter determining where change of bomb state is necessary*/
	p<unsigned> timer;
};

typedef persistent_ptr<list<bomb>> bomb_vec;

class player : public point {
public:
	player(position cor);
	void progress(int in, bomb_vec *bombs);
};

class alien : public point {
public:
	alien(position cor);
	void progress();
	void move_back_alien();

private:
	p<bool> rand_pos;
};

class intro : public point {
public:
	intro(int x, int y, direction d);
	void progress();

private:
	/* random color in which object will be displayed*/
	p<int> color;
	/* number determining object's path on the board*/
	p<int> num;
};

class board_state {
public:
	board_state(const std::string &map_file);
	~board_state();
	void reset_params();
	void reset_board();
	void print(unsigned hs);
	void reset();
	void dead();
	bool is_free(int x, int y);
	void set_board_elm(persistent_ptr<point> p);
	void add_points(int x, int y);
	bool is_last_alien_killed(int x, int y);
	void set_explosion(int x, int y, field f);
	void explosion(int x, int y, field f);
	inline field
	get_board_elm(int x, int y)
	{
		return board[y * SIZE + x];
	}
	inline void
	set_board_elm(int x, int y, field f)
	{
		board[y * SIZE + x] = f;
	}
	p<unsigned> level;     /* number of level */
	p<unsigned> timer;     /* measure time since game start */
	p<unsigned> n_aliens;  /* number of not killed aliens */
	p<unsigned> highscore; /* score of the best game */
	p<unsigned> score;     /* current score */
	p<bool> game_over;     /* set true if game is over */
private:
	int shape(field f);
	void set_bonus(field f);
	void set_board(const std::string &map_file);
	int find_wall(int x, int y, direction dir);
	p<unsigned> life;	      /* number of lives left for player */
	persistent_ptr<field[]> board; /* current state of board */
	persistent_ptr<field[]> board_tmpl; /* board template loaded from file*/
};

class state {
public:
	state();
	bool init(const std::string &map_file);
	void game();

private:
	bool intro_loop();
	void print_start();
	void print_game_over();
	void new_game(const std::string &map_file);
	void reset_game();
	void resume();
	void one_move(int in);
	void collision();
	void reset();
	void next_level();
	void reset_bombs();
	bool is_collision(persistent_ptr<point> p1, persistent_ptr<point> p2);
	/* pointer to player type object */
	persistent_ptr<player> pl;
	/* pointer to board state */
	persistent_ptr<board_state> board;
	/* pointer to vector of alien type objects */
	persistent_ptr<list<alien>> aliens;
	/* pointer to vector of intro type objects */
	persistent_ptr<list<intro>> intro_p;
	/* pointer to vector of bomb type objects */
	bomb_vec bombs;
	/* the best score player has ever achieved */
	p<unsigned> highscore;
};

/*
 * point::point -- overloaded constructor for point class
 */
point::point(int xf, int yf)
{
	x = xf;
	y = yf;
	prev_x = xf;
	prev_y = yf;
	prev_field = FREE;
};

/*
 * point::point -- overloaded constructor for point class
 */
point::point(position cor)
{
	home = cor;
	prev_field = FREE;
	move_home();
}

/*
 * point::move_home -- move object to it's home position
 */
void
point::move_home()
{
	prev_x = x;
	prev_y = y;

	switch (home) {
		case UP_LEFT:
			x = 1;
			y = 1;
			break;
		case UP_RIGHT:
			x = MAX_SIZE;
			y = 1;
			break;
		case DOWN_LEFT:
			x = 1;
			y = MAX_SIZE;
			break;
		case DOWN_RIGHT:
			x = MAX_SIZE;
			y = MAX_SIZE;
			break;
		case POS_MIDDLE:
			x = MAX_SIZE / 2;
			y = MAX_SIZE / 2;
			break;
		default:
			break;
	}
}

/*
 * point::move_back -- move object to it's previous position
 */
void
point::move_back()
{
	x = prev_x;
	y = prev_y;
}

/*
 * point::move -- move object in proper direction
 */
void
point::move()
{
	int tmp_x = 0, tmp_y = 0;
	switch (dir) {
		case LEFT:
			tmp_x = -1;
			break;
		case RIGHT:
			tmp_x = 1;
			break;
		case UP:
			tmp_y = -1;
			break;
		case DOWN:
			tmp_y = 1;
			break;
		default:
			break;
	}
	prev_x = x;
	prev_y = y;
	x = x + tmp_x;
	y = y + tmp_y;
}

/*
 * intro::intro -- overloaded constructor for intro class
 */
intro::intro(int x, int y, direction d) : point(x, y)
{
	color = (field)(rand() % BOMB);
	if (d == DOWN || d == LEFT)
		num = y;
	else
		num = SIZE - y;
	dir = d;
}

/*
 * intro::progress -- perform one move
 */
void
intro::progress()
{
	move();
	mvaddch(y, x * 2, COLOR_PAIR(color) | ACS_DIAMOND);
	int max_size = SIZE - num;
	if ((x == num && y == num) || (x == num && y == max_size) ||
	    (x == max_size && y == num) || (x == max_size && y == max_size))
		dir = (direction)((dir + 1) % STOP);
}

/*
 * bomb::bomb -- overloaded constructor for bomb class
 */
bomb::bomb(int xf, int yf) : point(xf, yf)
{
	cur_field = BOMB;
	exploded = false;
	used = false;
	timer = EXPLOSION_COUNTER;
}

/*
 * bomb::progress -- checks in which board_state is bomb
 */
void
bomb::progress()
{
	timer = timer - 1;
	if (exploded == false && timer == 0)
		explosion();
	else if (timer == 0)
		used = true;
}

/*
 * bomb::explosion -- change board_state of bomb on exploded
 */
void
bomb::explosion()
{
	exploded = true;
	timer = EXPLOSION_TIME;
}

/*
 * bomb::print_time -- print time to explosion
 */
void
bomb::print_time()
{
	if (!exploded)
		mvprintw(y, x * 2, "%u", timer / 10);
}

/*
 * player::player -- overloaded constructor for player class
 */
player::player(position cor) : point(cor)
{
	cur_field = PLAYER;
}

/*
 * player::progress -- checks input from keyboard and sets proper direction
 */
void
player::progress(int in, bomb_vec *bombs)
{
	switch (in) {
		case KEY_LEFT:
		case 'j':
			dir = LEFT;
			break;
		case KEY_RIGHT:
		case 'l':
			dir = RIGHT;
			break;
		case KEY_UP:
		case 'i':
			dir = UP;
			break;
		case KEY_DOWN:
		case 'k':
			dir = DOWN;
			break;
		case KEY_SPACE:
		case 'b':
			dir = STOP;
			if ((*bombs)->size() <= MAX_BOMBS)
				(*bombs)->push_back(
					make_persistent<bomb>(x, y));
			break;
	}
	move();
	dir = STOP;
}

/*
 * alien::alien -- overloaded constructor for alien class
 */
alien::alien(position cor) : point(cor)
{
	cur_field = ALIEN;
	prev_field = FOOD;
}

/*
 * alien::progress -- rand and set direction and move alien
 */
void
alien::progress()
{
	if (rand_pos || rand() % 10 == 0)
		dir = (direction)(rand() % STOP);
	rand_pos = false;
	move();
}

/*
 * alien::move_back_alien -- move alien to previous position
 */
void
alien::move_back_alien()
{
	rand_pos = true;
	move_back();
}

/*
 * board_state -- constructor for class board_state initializes boards and
 * needed variables
 */
board_state::board_state(const std::string &map_file)
{
	reset_params();
	board = make_persistent<field[]>(SIZE * SIZE);
	board_tmpl = make_persistent<field[]>(SIZE * SIZE);
	for (int i = 0; i < SIZE * SIZE; ++i)
		set_board_elm(i, 0, FREE);
	set_board(map_file);
}

board_state::~board_state()
{
	delete_persistent<field[]>(board, SIZE * SIZE);
	delete_persistent<field[]>(board_tmpl, SIZE * SIZE);
}

/* board_state::reset_params -- reset game parameters */
void
board_state::reset_params()
{
	life = 3;
	level = 1;
	n_aliens = 1;
	score = 0;
	timer = 0;
	game_over = false;
}

/* board_state::reset_board -- reset board state from template */
void
board_state::reset_board()
{
	for (auto i = 0; i < SIZE * SIZE; i++)
		board[i] = board_tmpl[i];

	set_bonus(BONUS);
	set_bonus(LIFE);
}

/*
 * board_state::print -- print current board and information about game
 */
void
board_state::print(unsigned hs)
{
	for (int i = 0; i < SIZE; i++) {
		for (int j = 0; j < SIZE; j++) {
			if (get_board_elm(j, i) != FREE)
				mvaddch(i, j * 2, shape(get_board_elm(j, i)));
		}
	}
	if (score > hs)
		highscore = score;
	mvprintw(SIZE + 1, 0, "Score: %d\t\tHighscore: %u\t\tLevel: %u\t"
			      "   Timer: %u",
		 score, highscore, level, timer);
	mvaddch(8, SIZE * 2 + 5, shape(FOOD));
	mvprintw(8, SIZE * 2 + 10, " +1 point");
	mvaddch(16, SIZE * 2 + 5, shape(BONUS));
	mvprintw(16, SIZE * 2 + 10, " +50 point");
	mvaddch(24, SIZE * 2 + 5, shape(ALIEN));
	mvprintw(24, SIZE * 2 + 10, " +100 point");
	mvaddch(32, SIZE * 2 + 5, shape(LIFE));
	mvprintw(32, SIZE * 2 + 10, " +1 life");

	for (unsigned i = 0; i < life; i++)
		mvaddch(SIZE + 3, SIZE + life - i * 2, shape(PLAYER));
}

/*
 * board_state::dead -- executed when player lose life
 */
void
board_state::dead()
{
	life = life - 1;
	if (life <= 0) {
		game_over = true;
	}
}

/*
 * board_state::reset -- clean board to start new level
 */
void
board_state::reset()
{
	reset_board();
	n_aliens = level;
	timer = 0;
}

/*
 * board_state::is_free -- check whether field is free
 */
bool
board_state::is_free(int x, int y)
{
	return !(get_board_elm(x, y) == WALL || get_board_elm(x, y) == BOMB);
}

/*
 * board_state::add_points -- check type of field and give proper number of
 * points
 */
void
board_state::add_points(int x, int y)
{
	switch (get_board_elm(x, y)) {
		case FOOD:
			score = score + 1;
			break;
		case BONUS:
			score = score + 50;
			set_bonus(BONUS);
			break;
		case LIFE:
			if (life < 3)
				life = life + 1;
			set_bonus(LIFE);
			break;
		default:
			break;
	}
}

/*
 * board_state::is_last_alien_killed -- remove alien from board and check
 * whether any other alien stayed on the board
 */
bool
board_state::is_last_alien_killed(int x, int y)
{
	set_board_elm(x, y, FREE);
	n_aliens = n_aliens - 1;
	score = score + 100;
	if (n_aliens != 0)
		return false;
	level = level + 1;
	return true;
}

/*
 * board_state::set_board_elm -- set object on its current position on the board
 * and clean previous position
 */
void
board_state::set_board_elm(persistent_ptr<point> p)
{
	set_board_elm(p->x, p->y, p->cur_field);
	if (!(p->x == p->prev_x && p->y == p->prev_y))
		set_board_elm(p->prev_x, p->prev_y, p->prev_field);
}

/*
 * board_state::set_explosion --set exploded fields in proper way
 */
void
board_state::set_explosion(int x, int y, field f)
{
	field prev_f = get_board_elm(x, y);
	if (prev_f == BONUS || prev_f == LIFE)
		set_bonus(prev_f);
	set_board_elm(x, y, f);
}

/*
 * board_state::explosion -- mark exploded fields as exploded or free
 */
void
board_state::explosion(int x, int y, field f)
{
	for (int i = find_wall(x, y, UP); i < find_wall(x, y, DOWN); i++)
		set_explosion(x, i, f);

	for (int i = find_wall(x, y, LEFT); i < find_wall(x, y, RIGHT); i++)
		set_explosion(i, y, f);
}

/*
 * board_state::shape -- assign proper shape to different types of fields
 */
int
board_state::shape(field f)
{
	int color = COLOR_PAIR(f);
	if (f == FOOD)
		return color | ACS_BULLET;
	else if (f == WALL || f == EXPLOSION)
		return color | ACS_CKBOARD;
	else
		return color | ACS_DIAMOND;
}

/*
 * board_state::set_bonus -- find free field and set the bonus there
 */
void
board_state::set_bonus(field f)
{

	int x, y;
	x = RAND_FIELD();
	y = RAND_FIELD();
	while (get_board_elm(x, y) != FOOD && get_board_elm(x, y) != FREE) {
		x = RAND_FIELD();
		y = RAND_FIELD();
	}
	set_board_elm(x, y, f);
}

/*
 * board_state::set_board -- set board with initial values from file
 */
void
board_state::set_board(const std::string &map_file)
{
	std::ifstream board_file;
	board_file.open(map_file.c_str());
	if (!board_file)
		assert(0);
	char num;
	for (unsigned i = 0; i < SIZE; i++) {
		for (unsigned j = 0; j < SIZE; j++) {
			board_file.get(num);
			if (num == '#')
				set_board_elm(j, i, WALL);
			else if (num == ' ')
				set_board_elm(j, i, FOOD);
			else
				set_board_elm(j, i, FREE);
		}
		board_file.get(num);
	}
	for (auto i = 0; i < SIZE * SIZE; i++)
		board_tmpl[i] = board[i];

	board_file.close();
	set_bonus(BONUS);
	set_bonus(LIFE);
}

/*
 * board_state::find_wall -- finds first wall from given point in given
 * direction
 */
int
board_state::find_wall(int x, int y, direction dir)
{
	switch (dir) {
		case LEFT: {
			for (int i = x; i >= 0; i--) {
				if (get_board_elm(i, y) == WALL)
					return i + 1;
			}
			break;
		}
		case RIGHT: {
			for (int i = x; i <= SIZE; i++) {
				if (get_board_elm(i, y) == WALL)
					return i;
			}
			break;
		}
		case UP: {
			for (int i = y; i >= 0; i--) {
				if (get_board_elm(x, i) == WALL)
					return i + 1;
			}
			break;
		}
		case DOWN: {
			for (int i = y; i <= SIZE; i++) {
				if (get_board_elm(x, i) == WALL)
					return i;
			}
			break;
		}
		default:
			break;
	}
	return 0;
}

/*
 * state::init -- initialize game
 */
bool
state::init(const std::string &map_file)
{
	int in;
	if (board == nullptr || pl == nullptr)
		new_game(map_file);
	else {
		while ((in = getch()) != 'y') {
			mvprintw(SIZE / 4, SIZE / 4,
				 "Do you want to continue the game?"
				 " [y/n]");
			if (in == 'n') {
				resume();
				break;
			}
		}
		if (in == 'y' && intro_p->size() == 0)
			return false;
	}

	try {
		transaction::manual tx(pop);
		if (intro_p->size() == 0) {
			for (int i = 0; i < SIZE / 4; i++) {
				intro_p->push_back(
					make_persistent<intro>(i, i, DOWN));
				intro_p->push_back(make_persistent<intro>(
					SIZE - i, i, LEFT));
				intro_p->push_back(make_persistent<intro>(
					i, SIZE - i, RIGHT));
				intro_p->push_back(make_persistent<intro>(
					SIZE - i, SIZE - i, UP));
			}
		}
		transaction::commit();
	} catch (transaction_error &err) {
		std::cout << err.what() << std::endl;
		assert(0);
	}

	if (intro_loop() == true)
		return true;

	try {
		transaction::manual tx(pop);
		intro_p->clear();
		transaction::commit();
	} catch (transaction_error &err) {
		std::cout << err.what() << std::endl;
		assert(0);
	}
	return false;
}

/*
 * state::game -- process game loop
 */
void
state::game()
{
	int in;
	while ((in = getch()) != 'q') {
		SLEEP(GAME_DELAY);
		erase();
		if (in == 'r')
			resume();

		if (!board->game_over)
			one_move(in);
		else
			print_game_over();
	}
}

/*
 * state::intro_loop -- display intro an wait for user's reaction
 */
bool
state::intro_loop()
{
	int in;
	while ((in = getch()) != 's') {
		print_start();
		unsigned i = 0;
		persistent_ptr<intro> p;
		try {
			transaction::manual tx(pop);
			while ((p = intro_p->get(i++)) != nullptr)
				p->progress();
			transaction::commit();
		} catch (transaction_error &err) {
			std::cout << err.what() << std::endl;
			assert(0);
		}
		SLEEP(GAME_DELAY);
		if (in == 'q')
			return true;
	}
	return false;
}

/*
 * state::print_start -- print intro inscription
 */
void
state::print_start()
{
	erase();
	int x = SIZE / 1.8;
	int y = SIZE / 2.5;
	mvprintw(y + 0, x, "#######   #     #   #######   #    #");
	mvprintw(y + 1, x, "#     #   ##   ##   #     #   ##   #");
	mvprintw(y + 2, x, "#######   # # # #   #######   # #  #");
	mvprintw(y + 3, x, "#         #  #  #   #     #   #  # #");
	mvprintw(y + 4, x, "#         #     #   #     #   #   ##");
	mvprintw(y + 8, x, "          Press 's' to start        ");
	mvprintw(y + 9, x, "          Press 'q' to quit        ");
}

/*
 * state::print_game_over -- print game over inscription
 */
void
state::print_game_over()
{
	erase();
	int x = SIZE / 3;
	int y = SIZE / 6;
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

	mvprintw(y + 13, x, "       Your final score is %u         ",
		 board->score);
	if (board->score == highscore)
		mvprintw(y + 14, x, "       YOU BET YOUR BEST SCORE!       ");
	mvprintw(y + 16, x, "          Press 'q' to quit           ");
	mvprintw(y + 17, x, "         Press 'r' to resume          ");
}
/*
 * state::new_game -- allocate board_state, player and aliens if root is empty
 */
void
state::new_game(const std::string &map_file)
{
	try {
		transaction::manual tx(pop);

		board = make_persistent<board_state>(map_file);
		pl = make_persistent<player>(POS_MIDDLE);
		intro_p = make_persistent<list<intro>>();
		bombs = make_persistent<list<bomb>>();
		aliens = make_persistent<list<alien>>();
		aliens->push_back(make_persistent<alien>(UP_LEFT));

		transaction::commit();
	} catch (transaction_error &err) {
		std::cout << err.what() << std::endl;
		assert(0);
	}
}

/*
 * state::reset_game -- reset the game from the board template
 */
void
state::reset_game()
{
	try {
		transaction::manual tx(pop);

		board->reset_params();
		board->reset_board();
		pl = make_persistent<player>(POS_MIDDLE);
		intro_p = make_persistent<list<intro>>();
		bombs = make_persistent<list<bomb>>();
		aliens = make_persistent<list<alien>>();
		aliens->push_back(make_persistent<alien>(UP_LEFT));

		transaction::commit();
	} catch (transaction_error &err) {
		std::cout << err.what() << std::endl;
		assert(0);
	}
}

/*
 * state::resume -- clean root pointer and start a new game
 */
void
state::resume()
{
	try {
		transaction::manual tx(pop);
		delete_persistent<player>(pl);
		pl = nullptr;

		aliens->clear();
		delete_persistent<list<alien>>(aliens);

		bombs->clear();
		delete_persistent<list<bomb>>(bombs);

		intro_p->clear();
		delete_persistent<list<intro>>(intro_p);
		transaction::commit();
	} catch (transaction_error &err) {
		std::cout << err.what() << std::endl;
		assert(0);
	}
	reset_game();
}

/*
 * state::one_move -- process one round where every object moves one time
 */
void
state::one_move(int in)
{
	unsigned i = 0;
	persistent_ptr<alien> a;
	persistent_ptr<bomb> b;
	try {
		transaction::manual tx(pop);
		board->timer = board->timer + 1;
		pl->progress(in, &bombs);
		while ((a = aliens->get(i++)) != nullptr)
			a->progress();
		i = 0;
		while ((b = bombs->get(i++)) != nullptr) {
			b->progress();
			if (b->exploded)
				board->explosion(b->x, b->y, EXPLOSION);
			if (b->used) {
				board->explosion(b->x, b->y, FREE);
				bombs->erase(--i);
			}
		}
		collision();
		board->print(highscore);
		highscore = board->highscore;
		i = 0;
		while ((b = bombs->get(i++)) != nullptr)
			b->print_time();
		transaction::commit();
	} catch (transaction_error &err) {
		std::cout << err.what() << std::endl;
		assert(0);
	}
}

/*
 * state::collision -- check for collisions between any two objects
 */
void
state::collision()
{
	unsigned i = 0;
	persistent_ptr<alien> a;
	persistent_ptr<bomb> b;
	while ((b = bombs->get(i++)) != nullptr) {
		if (!b->exploded) {
			if (board->get_board_elm(b->x, b->y) == EXPLOSION)
				b->explosion();
			board->set_board_elm(b);
		}
	}
	i = 0;
	while ((a = aliens->get(i++)) != nullptr) {
		if (board->get_board_elm(a->x, a->y) == EXPLOSION) {
			bool is_over = board->is_last_alien_killed(a->prev_x,
								   a->prev_y);
			aliens->erase(--i);
			if (is_over == true) {
				if (board->get_board_elm(pl->x, pl->y) ==
				    EXPLOSION)
					board->dead();
				next_level();
				return;
			}
		}
	}
	bool dead = false;
	i = 0;
	while ((a = aliens->get(i++)) != nullptr) {

		/*check collision alien with wall or bomb */
		if (!board->is_free(a->x, a->y))
			a->move_back_alien();

		/*check collision alien with player */
		if (is_collision(pl, a))
			dead = true;

		/*check collision alien with alien */
		unsigned j = 0;
		persistent_ptr<alien> a2;
		while ((a2 = aliens->get(j++)) != nullptr) {
			if (a != a2 && is_collision(a, a2)) {
				a->move_back_alien();
				break;
			}
		}
		field prev_f = board->get_board_elm(a->x, a->y);
		board->set_board_elm(a);
		if (prev_f != ALIEN && prev_f != PLAYER)
			a->prev_field = prev_f;
	}
	if (!board->is_free(pl->x, pl->y))
		pl->move_back();

	if (board->get_board_elm(pl->x, pl->y) == EXPLOSION || dead) {
		board->dead();
		reset();
		return;
	}
	board->add_points(pl->x, pl->y);
	board->set_board_elm(pl);
	SLEEP(10000);
}

/*
 * state::reset -- move objects on their home positions
 */
void
state::reset()
{
	unsigned i = 0;
	persistent_ptr<alien> a;
	while ((a = aliens->get(i++)) != nullptr) {
		a->move_home();
		board->set_board_elm(a);
	}
	pl->move_back();
	pl->move_home();
	board->set_board_elm(pl);
	reset_bombs();
}

/*
 * state::next_level -- clean board, create proper number of aliens and
 * start new level
 */
void
state::next_level()
{
	reset_bombs();
	board->reset();
	for (unsigned i = 0; i < board->n_aliens; i++)
		aliens->push_back(make_persistent<alien>(
			(position)((UP_LEFT + i) % (POS_MAX - 1))));
	pl->move_home();
}

/*
 * state::reset_bombs -- remove all bombs
 */
void
state::reset_bombs()
{
	unsigned i = 0;
	persistent_ptr<bomb> b;
	while ((b = bombs->get(i++)) != nullptr) {
		if (b->exploded)
			board->explosion(b->x, b->y, FREE);
	}
	bombs->clear();
}

/*
 * state::is_collision -- check if there is collision between given objects
 */
bool
state::is_collision(persistent_ptr<point> p1, persistent_ptr<point> p2)
{
	if (p1->x == p2->x && p1->y == p2->y)
		return true;
	else if (p1->prev_x == p2->x && p1->prev_y == p2->y &&
		 p1->x == p2->prev_x && p1->y == p2->prev_y)
		return true;
	return false;
}

} /* namespace examples */

namespace
{

void
print_usage(const std::string &binary)
{
	std::cout << "Usage:\n" << binary << " <game_file> [map_file]\n";
}
}

int
main(int argc, char *argv[])
{
	if (argc < 2 || argc > 3) {
		print_usage(argv[0]);
		return 1;
	}

	std::string name = argv[1];
	std::string map_path = "map";

	if (argc == 3)
		map_path = argv[2];

	if (pool<examples::state>::check(name, LAYOUT_NAME) == 1)
		pop = pool<examples::state>::open(name, LAYOUT_NAME);
	else
		pop = pool<examples::state>::create(name, LAYOUT_NAME);

	initscr();
	start_color();
	init_pair(examples::FOOD, COLOR_YELLOW, COLOR_BLACK);
	init_pair(examples::WALL, COLOR_WHITE, COLOR_BLACK);
	init_pair(examples::PLAYER, COLOR_CYAN, COLOR_BLACK);
	init_pair(examples::ALIEN, COLOR_RED, COLOR_BLACK);
	init_pair(examples::EXPLOSION, COLOR_CYAN, COLOR_BLACK);
	init_pair(examples::BONUS, COLOR_YELLOW, COLOR_BLACK);
	init_pair(examples::LIFE, COLOR_MAGENTA, COLOR_BLACK);
	nodelay(stdscr, true);
	curs_set(0);
	keypad(stdscr, true);
	persistent_ptr<examples::state> r = pop.get_root();
	if (r == nullptr)
		goto out;

	if (r->init(map_path))
		goto out;
	r->game();
out:
	endwin();
	pop.close();
	return 0;
}
