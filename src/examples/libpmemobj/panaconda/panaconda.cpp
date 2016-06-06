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
#include <getopt.h>
#include <iostream>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>

#include "panaconda.hpp"

#define LAYOUT_NAME "panaconda"
#define DEFAULT_DELAY 120000

#define SNAKE_START_POS_X 5
#define SNAKE_START_POS_Y 5
#define SNAKE_START_DIR (direction::RIGHT)
#define SNAKE_STAR_SEG_NO 5

#define BOARD_STATIC_SIZE_ROW 40
#define BOARD_STATIC_SIZE_COL 30

#define PLAYER_POINTS_PER_HIT 10

using nvml::transaction_error;
using nvml::obj::transaction;
using nvml::obj::persistent_ptr;
using nvml::obj::pool;
using nvml::obj::make_persistent;
using nvml::obj::delete_persistent;
using examples::list;

/*
 * Color_pair
 */
color_pair::color_pair() : color_bg(COLOR_BLACK), color_fg(COLOR_BLACK)
{
}
color_pair::color_pair(const int col_fg, const int col_bg)
    : color_bg(col_bg), color_fg(col_fg)
{
}

/*
 * Helper
 */
struct color_pair
helper::get_color(const object_type obj_type)
{
	struct color_pair res;
	switch (obj_type) {
		case SNAKE_SEGMENT:
			res = color_pair(COLOR_WHITE, COLOR_BLACK);
			break;
		case WALL:
			res = color_pair(COLOR_BLUE, COLOR_BLUE);
			break;
		case FOOD:
			res = color_pair(COLOR_RED, COLOR_BLACK);
			break;
		default:
			std::cout << "Error: get_color - wrong value passed!"
				  << std::endl;
			assert(0);
	}
	return res;
}

int
helper::parse_params(int argc, char *argv[], struct parameters *par)
{
	int opt;
	std::string app = argv[0];
	while ((opt = getopt(argc, argv, "m:")) != -1) {
		switch (opt) {
			case 'm':
				par->use_maze = true;
				par->maze_path = optarg;
				break;
			default:
				helper::print_usage(app);
				return -1;
		}
	}

	if (optind < argc) {
		par->name = argv[optind];
	} else {
		helper::print_usage(app);
		return -1;
	}
	return 0;
}

inline void
helper::sleep(int time)
{
	clock_t curr_time = clock();
	while (clock() < (curr_time + time)) {
	}
}

inline void
helper::print_usage(std::string &name)
{
	std::cout << "Usage: " << name << " [-m <maze_path>] <pool_name>\n";
}

/*
 * Point
 */
point::point() : x(0), y(0)
{
}

point::point(int x, int y) : x(x), y(y)
{
}

bool
operator==(point &point1, point &point2)
{
	return point1.x == point2.x && point1.y == point2.y;
}
/*
 * Shape
 */
element_shape::element_shape(int shape)
{
	int n_curves_symbol = get_symbol(shape);
	val = COLOR_PAIR(shape) | n_curves_symbol;
}

int
element_shape::get_val()
{
	return val;
}

int
element_shape::get_symbol(int shape)
{
	int symbol = 0;
	switch (shape) {
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

/*
 * Element
 */
board_element::board_element()
    : position(make_persistent<point>(0, 0)),
      shape(make_persistent<element_shape>(SNAKE_SEGMENT)),
      element_dir(direction::LEFT)
{
}

board_element::board_element(int px, int py,
			     nvml::obj::persistent_ptr<element_shape> shape,
			     direction dir)
    : position(make_persistent<point>(px, py)), shape(shape), element_dir(dir)
{
}

board_element::board_element(point p,
			     nvml::obj::persistent_ptr<element_shape> shape,
			     direction dir)
    : position(make_persistent<point>(p.x, p.y)), shape(shape), element_dir(dir)
{
}

board_element::board_element(const board_element &element)
{
	position = make_persistent<point>(element.position->x,
					  element.position->y);
	shape = make_persistent<element_shape>(element.shape->get_val());
}

board_element::~board_element()
{
	nvml::obj::delete_persistent<point>(position);
	position = nullptr;
	nvml::obj::delete_persistent<element_shape>(shape);
	shape = nullptr;
}

persistent_ptr<point>
board_element::calc_new_position(const direction dir)
{
	persistent_ptr<point> pt =
		make_persistent<point>(position->x, position->y);

	switch (dir) {
		case direction::DOWN:
			pt->y = pt->y + 1;
			break;
		case direction::LEFT:
			pt->x = pt->x - 1;
			break;
		case direction::RIGHT:
			pt->x = pt->x + 1;
			break;
		case direction::UP:
			pt->y = pt->y - 1;
			break;
		default:
			break;
	}

	return pt;
}

void
board_element::set_position(const persistent_ptr<point> new_point)
{
	position = new_point;
}

persistent_ptr<point>
board_element::get_position(void)
{
	return position;
}

void
board_element::print(void)
{
	mvaddch(position->y, position->x, shape->get_val());
}

void
board_element::print_double_col(void)
{
	mvaddch(position->y, (2 * position->x), shape->get_val());
}

void
board_element::print_single_double_col(void)
{
	mvaddch(position->y, (2 * position->x), shape->get_val());
	mvaddch(position->y, (2 * position->x - 1), shape->get_val());
}

direction
board_element::get_direction(void)
{
	return element_dir;
}
void
board_element::set_direction(const direction dir)
{
	element_dir = dir;
}

/*
 * Snake
 */
snake::snake()
{
	snake_segments = make_persistent<list<board_element>>();
	for (unsigned i = 0; i < SNAKE_STAR_SEG_NO; ++i) {
		persistent_ptr<element_shape> shape =
			make_persistent<element_shape>(SNAKE_SEGMENT);
		persistent_ptr<board_element> element =
			make_persistent<board_element>(SNAKE_START_POS_X - i,
						       SNAKE_START_POS_Y, shape,
						       SNAKE_START_DIR);
		snake_segments->push_back(element);
	}

	last_seg_position = point();
	last_seg_dir = direction::RIGHT;
}

snake::~snake()
{
	snake_segments->clear();
	delete_persistent<list<board_element>>(snake_segments);
}

void
snake::move(const direction dir)
{
	int snake_size = snake_segments->size();
	persistent_ptr<point> new_position_point;

	last_seg_position =
		*(snake_segments->get(snake_size - 1)->get_position().get());
	last_seg_dir = snake_segments->get(snake_size - 1)->get_direction();

	for (int i = (snake_size - 1); i >= 0; --i) {
		if (i == 0) {
			new_position_point =
				snake_segments->get(i)->calc_new_position(dir);
			snake_segments->get(i)->set_direction(dir);
		} else {
			new_position_point =
				snake_segments->get(i)->calc_new_position(
					snake_segments->get(i - 1)
						->get_direction());
			snake_segments->get(i)->set_direction(
				snake_segments->get(i - 1)->get_direction());
		}
		snake_segments->get(i)->set_position(new_position_point);
	}
}

void
snake::print(void)
{
	int i = 0;
	persistent_ptr<board_element> segp;

	while ((segp = snake_segments->get(i++)) != nullptr)
		segp->print_double_col();
}

void
snake::add_segment(void)
{
	persistent_ptr<element_shape> shape =
		make_persistent<element_shape>(SNAKE_SEGMENT);
	persistent_ptr<board_element> segp = make_persistent<board_element>(
		last_seg_position, shape, last_seg_dir);
	snake_segments->push_back(segp);
}

bool
snake::check_point_against_segments(point point)
{
	int i = 0;
	bool result = false;
	persistent_ptr<board_element> segp;

	while ((segp = snake_segments->get(i++)) != nullptr) {
		if (point == *(segp->get_position().get())) {
			result = true;
			break;
		}
	}
	return result;
}

point
snake::get_head_point(void)
{
	return *(snake_segments->get(0)->get_position().get());
}

direction
snake::get_direction(void)
{
	return snake_segments->get(0)->get_direction();
}

point
snake::get_next_point(const direction dir)
{
	return *(snake_segments->get(0)->calc_new_position(dir).get());
}

/*
 * Board
 */

game_board::game_board()
{
	persistent_ptr<element_shape> shape =
		make_persistent<element_shape>(FOOD);
	food = make_persistent<board_element>(0, 0, shape,
					      direction::UNDEFINED);
	layout = make_persistent<list<board_element>>();
	anaconda = make_persistent<snake>();
	size_row = 20;
	size_col = 20;
}

game_board::~game_board()
{
	layout->clear();
	delete_persistent<list<board_element>>(layout);
	delete_persistent<snake>(anaconda);
	delete_persistent<board_element>(food);
}

void
game_board::print(const int score)
{
	const int offset_y = 2 * size_col + 5;
	const int offset_x = 2;

	int i = 0;
	persistent_ptr<board_element> elmp;

	while ((elmp = layout->get(i++)) != nullptr)
		elmp->print_single_double_col();

	anaconda->print();
	food->print_double_col();

	mvprintw((offset_x + 0), offset_y, " ##### panaconda ##### ");
	mvprintw((offset_x + 1), offset_y, " #                   # ");
	mvprintw((offset_x + 2), offset_y, " #    q - quit       # ");
	mvprintw((offset_x + 3), offset_y, " #    n - new game   # ");
	mvprintw((offset_x + 4), offset_y, " #                   # ");
	mvprintw((offset_x + 5), offset_y, " ##################### ");

	mvprintw((offset_x + 7), offset_y, " Score: %d ", score);
}

void
game_board::print_game_over(const int score)
{
	int x = size_col / 3;
	int y = size_row / 6;
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

	mvprintw(y + 12, x, " Last score: %d ", score);
	mvprintw(y + 14, x, " q - quit");
	mvprintw(y + 15, x, " n - new game");
}

int
game_board::creat_dynamic_layout(const unsigned row_no, char *const buffer)
{
	persistent_ptr<board_element> element;
	persistent_ptr<element_shape> shape;

	for (unsigned i = 0; i < size_col; ++i) {
		if (buffer[i] == config_file_symbol::SYM_WALL) {
			shape = make_persistent<element_shape>(WALL);
			element = element = make_persistent<board_element>(
				i, row_no, shape, direction::UNDEFINED);
			layout->push_back(element);
		}
	}
	return 0;
}

int
game_board::creat_static_layout(void)
{
	persistent_ptr<board_element> element;
	persistent_ptr<element_shape> shape;

	size_row = BOARD_STATIC_SIZE_ROW;
	size_col = BOARD_STATIC_SIZE_COL;

	// first and last row
	for (unsigned i = 0; i < size_col; ++i) {
		shape = make_persistent<element_shape>(WALL);
		element = make_persistent<board_element>(i, 0, shape,
							 direction::UNDEFINED);
		layout->push_back(element);
		shape = make_persistent<element_shape>(WALL);
		element = make_persistent<board_element>(
			i, (size_row - 1), shape, direction::UNDEFINED);
		layout->push_back(element);
	}

	// middle rows
	for (unsigned i = 1; i < size_row; ++i) {
		shape = make_persistent<element_shape>(WALL);
		element = make_persistent<board_element>(0, i, shape,
							 direction::UNDEFINED);
		layout->push_back(element);
		shape = make_persistent<element_shape>(WALL);
		element = make_persistent<board_element>(
			(size_col - 1), i, shape, direction::UNDEFINED);
		layout->push_back(element);
	}
	return 0;
}

bool
game_board::is_snake_collision(point point)
{
	return anaconda->check_point_against_segments(point);
}

bool
game_board::is_wall_collision(point point)
{
	int i = 0;
	bool result = false;
	persistent_ptr<board_element> wallp;

	while ((wallp = layout->get(i++)) != nullptr) {
		if (point == *(wallp->get_position().get())) {
			result = true;
			break;
		}
	}
	return result;
}

bool
game_board::is_collision(point point)
{
	return is_snake_collision(point) || is_wall_collision(point);
}

bool
game_board::is_snake_head_food_hit(void)
{
	bool result = false;
	point head_point = anaconda->get_head_point();

	if (head_point == *(food->get_position().get())) {
		result = true;
	}
	return result;
}

void
game_board::set_new_food(const point point)
{
	persistent_ptr<element_shape> shape =
		make_persistent<element_shape>(FOOD);
	delete_persistent<board_element>(food);
	food = make_persistent<board_element>(point, shape,
					      direction::UNDEFINED);
}

void
game_board::create_new_food(void)
{
	const int max_repeat = 50;
	int count = 0;
	int rand_row = 0;
	int rand_col = 0;

	while (count < max_repeat) {
		rand_row = 1 + rand() % (get_size_row() - 2);
		rand_col = 1 + rand() % (get_size_col() - 2);

		point food_point(rand_col, rand_row);
		if (!is_collision(food_point)) {
			set_new_food(food_point);
			break;
		}
		count++;
	}
}

snake_event
game_board::move_snake(const direction dir)
{
	snake_event event = snake_event::EV_OK;
	point next_pt = anaconda->get_next_point(dir);

	if (is_collision(next_pt)) {
		event = snake_event::EV_COLLISION;
	} else {
		anaconda->move(dir);
	}

	return event;
}

void
game_board::add_snake_segment(void)
{
	anaconda->add_segment();
}

unsigned
game_board::get_size_row(void)
{
	return size_row;
}

void
game_board::set_size_row(const unsigned size_r)
{
	size_row = size_r;
}

unsigned
game_board::get_size_col(void)
{
	return size_col;
}

void
game_board::set_size_col(const unsigned size_c)
{
	size_col = size_c;
}

direction
game_board::get_snake_dir(void)
{
	return anaconda->get_direction();
}

/*
 * Game_state
 */
game_state::game_state()
{
}

game_state::~game_state()
{
}

nvml::obj::persistent_ptr<game_board>
game_state::get_board()
{
	return board;
}

nvml::obj::persistent_ptr<game_player>
game_state::get_player()
{
	return player;
}

void
game_state::init(void)
{
	board = make_persistent<game_board>();
	player = make_persistent<game_player>();
}

void
game_state::clean_pool(void)
{
	delete_persistent<game_board>(board);
	board = nullptr;

	delete_persistent<game_player>(player);
	player = nullptr;
}

/*
 * Player
 */
game_player::game_player() : score(0), state(STATE_PLAY)
{
}

game_player::~game_player()
{
}

int
game_player::get_score(void)
{
	return score;
}

play_state
game_player::get_state(void)
{
	return state;
}
void
game_player::set_state(const play_state st)
{
	state = st;
}

void
game_player::update_score(void)
{
	score = score + PLAYER_POINTS_PER_HIT;
}
/*
 * Game
 */
game::game(struct parameters *par)
{
	pool<game_state> pop;

	initscr();
	start_color();
	nodelay(stdscr, true);
	curs_set(0);
	keypad(stdscr, true);

	params = par;
	if (pool<game_state>::check(params->name, LAYOUT_NAME) == 1)
		pop = pool<game_state>::open(params->name, LAYOUT_NAME);
	else
		pop = pool<game_state>::create(params->name, LAYOUT_NAME,
					       PMEMOBJ_MIN_POOL * 10, 0666);

	state = pop;
	direction_key = direction::UNDEFINED;
	last_key = KEY_CLEAR;
	delay = DEFAULT_DELAY;

	init_colors();

	srand(time(0));
}

game::~game()
{
}

void
game::init_colors(void)
{
	struct color_pair color_pair = helper::get_color(SNAKE_SEGMENT);
	init_pair(SNAKE_SEGMENT, color_pair.color_fg, color_pair.color_bg);

	color_pair = helper::get_color(WALL);
	init_pair(WALL, color_pair.color_fg, color_pair.color_bg);

	color_pair = helper::get_color(FOOD);
	init_pair(FOOD, color_pair.color_fg, color_pair.color_bg);
}

int
game::init(void)
{
	int ret = 0;
	persistent_ptr<game_state> r = state.get_root();

	if (r->get_board() == nullptr) {
		try {
			transaction::manual tx(state);
			r->init();
			if (params->use_maze)
				ret = parse_conf_create_dynamic_layout();
			else
				ret = r->get_board()->creat_static_layout();

			r->get_board()->create_new_food();
			transaction::commit();
		} catch (transaction_error &err) {
			std::cout << err.what() << std::endl;
		}
		if (ret) {
			clean_pool();
			clear_prog();
			std::cout << "Error: Config file error!" << std::endl;
			return ret;
		}
	}
	direction_key = r->get_board()->get_snake_dir();
	return ret;
}

void
game::process_step(void)
{
	snake_event ret_event = EV_OK;
	persistent_ptr<game_state> r = state.get_root();
	try {
		transaction::exec_tx(state, [&]() {
			ret_event = r->get_board()->move_snake(direction_key);
			if (EV_COLLISION == ret_event) {
				r->get_player()->set_state(
					play_state::STATE_GAMEOVER);
				return;
			} else {
				if (r->get_board()->is_snake_head_food_hit()) {
					r->get_board()->create_new_food();
					r->get_board()->add_snake_segment();
					r->get_player()->update_score();
				}
			}
		});
	} catch (transaction_error &err) {
		std::cout << err.what() << std::endl;
	}

	r->get_board()->print(r->get_player()->get_score());
}

inline bool
game::is_stopped(void)
{
	return action::ACTION_QUIT == last_key;
}

void
game::set_direction_key(void)
{
	switch (last_key) {
		case KEY_LEFT:
			if (direction::RIGHT != direction_key)
				direction_key = direction::LEFT;
			break;
		case KEY_RIGHT:
			if (direction::LEFT != direction_key)
				direction_key = direction::RIGHT;
			break;
		case KEY_UP:
			if (direction::DOWN != direction_key)
				direction_key = direction::UP;
			break;
		case KEY_DOWN:
			if (direction::UP != direction_key)
				direction_key = direction::DOWN;
			break;
		default:
			break;
	}
}

int
game::process_key(const int lastkey)
{
	int ret = 0;
	last_key = lastkey;
	set_direction_key();

	if (action::ACTION_NEW_GAME == last_key) {
		clean_pool();
		ret = init();
	}
	return ret;
}

void
game::clean_pool(void)
{
	persistent_ptr<game_state> r = state.get_root();

	try {
		transaction::exec_tx(state, [&]() { r->clean_pool(); });
	} catch (transaction_error &err) {
		std::cout << err.what() << std::endl;
	}
}

void
game::process_delay(void)
{
	helper::sleep(delay);
}

void
game::clear_screen(void)
{
	erase();
}

void
game::game_over(void)
{
	persistent_ptr<game_state> r = state.get_root();
	r->get_board()->print_game_over(r->get_player()->get_score());
}

bool
game::is_game_over(void)
{
	persistent_ptr<game_state> r = state.get_root();
	return (r->get_player()->get_state() == play_state::STATE_GAMEOVER);
}

void
game::clear_prog(void)
{
	state.close();
	endwin();
}

int
game::parse_conf_create_dynamic_layout(void)
{
	FILE *cfg_file;
	char *line = NULL;
	size_t len = 0;
	unsigned i = 0;
	ssize_t col_no = 0;

	cfg_file = fopen(params->maze_path.c_str(), "r");
	if (cfg_file == NULL)
		return -1;

	persistent_ptr<game_state> r = state.get_root();
	while ((col_no = getline(&line, &len, cfg_file)) != -1) {
		if (i == 0)
			r->get_board()->set_size_col(col_no - 1);

		try {
			transaction::exec_tx(state, [&]() {
				r->get_board()->creat_dynamic_layout(i, line);
			});
		} catch (transaction_error &err) {
			std::cout << err.what() << std::endl;
		}
		i++;
	}
	r->get_board()->set_size_row(i);

	free(line);
	fclose(cfg_file);
	return 0;
}

/*
 * main
 */
int
main(int argc, char *argv[])
{
	int input;
	game *snake_game;
	struct parameters params;
	params.use_maze = false;

	if (helper::parse_params(argc, argv, &params))
		return -1;

	snake_game = new game(&params);
	if (snake_game->init())
		return -1;

	while (!snake_game->is_stopped()) {
		input = getch();
		if (snake_game->process_key(input))
			return -1;
		if (snake_game->is_game_over()) {
			snake_game->game_over();
		} else {
			snake_game->process_delay();
			snake_game->clear_screen();
			snake_game->process_step();
		}
	}

	snake_game->clear_prog();
	return 0;
}
