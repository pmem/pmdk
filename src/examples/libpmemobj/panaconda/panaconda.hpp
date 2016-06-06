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

class board_element;

enum direction { UNDEFINED, DOWN, RIGHT, UP, LEFT };
enum object_type { SNAKE_SEGMENT, WALL, FOOD };
enum config_file_symbol { SYM_NOTHING = '0', SYM_WALL = '1' };
enum play_state { STATE_NEW, STATE_PLAY, STATE_GAMEOVER };

enum snake_event { EV_OK, EV_COLLISION };

enum action { ACTION_NEW_GAME = 'n', ACTION_QUIT = 'q' };

typedef nvml::obj::persistent_ptr<examples::list<board_element>> element_list;

struct color_pair {
	color_pair();
	color_pair(const int col_fg, const int col_bg);
	int color_bg;
	int color_fg;
};

struct parameters {
	bool use_maze;
	std::string name;
	std::string maze_path;
};

class helper {
public:
	static color_pair get_color(const object_type obj_type);
	static int parse_params(int argc, char *argv[],
				struct parameters *params);
	static inline void sleep(int time);
	static inline void print_usage(std::string &name);
};

class point {
public:
	nvml::obj::p<int> x;
	nvml::obj::p<int> y;

	point();
	point(int x, int y);
	friend bool operator==(point &point1, point &point2);
};

bool operator==(point &point1, point &point2);

class element_shape {
public:
	element_shape() = default;
	element_shape(int shape);
	int get_val();

private:
	nvml::obj::p<int> val;
	int get_symbol(int shape);
};

class board_element {
public:
	board_element();
	board_element(int px, int py,
		      nvml::obj::persistent_ptr<element_shape> shape,
		      direction dir);
	board_element(point p, nvml::obj::persistent_ptr<element_shape> shape,
		      direction dir);
	board_element(const board_element &element);
	~board_element();

	nvml::obj::persistent_ptr<point> calc_new_position(const direction dir);
	void print(void);
	void print_double_col(void);
	void print_single_double_col(void);
	nvml::obj::persistent_ptr<point> get_position(void);
	void set_position(const nvml::obj::persistent_ptr<point> new_point);
	direction get_direction(void);
	void set_direction(const direction dir);

private:
	nvml::obj::persistent_ptr<point> position;
	nvml::obj::persistent_ptr<element_shape> shape;
	nvml::obj::p<direction> element_dir;
};

class snake {
public:
	snake();
	~snake();

	void move(const direction dir);
	void print(void);
	void add_segment(void);
	bool check_point_against_segments(point point);
	point get_head_point(void);
	direction get_direction(void);
	point get_next_point(const direction dir);

private:
	element_list snake_segments;
	nvml::obj::p<point> last_seg_position;
	nvml::obj::p<direction> last_seg_dir;
};

class game_board {
public:
	game_board();
	~game_board();
	void print(const int score);
	void print_game_over(const int score);
	unsigned get_size_row(void);
	void set_size_row(const unsigned size_r);
	unsigned get_size_col(void);
	void set_size_col(const unsigned size_c);
	int creat_dynamic_layout(const unsigned row_no, char *const buffer);
	int creat_static_layout(void);
	bool is_snake_head_food_hit(void);
	void create_new_food(void);
	bool is_collision(point point);
	snake_event move_snake(const direction dir);
	direction get_snake_dir(void);
	void add_snake_segment(void);

private:
	nvml::obj::persistent_ptr<snake> anaconda;
	nvml::obj::persistent_ptr<board_element> food;
	element_list layout;

	nvml::obj::p<unsigned> size_row;
	nvml::obj::p<unsigned> size_col;

	void set_new_food(const point point);
	bool is_snake_collision(point point);
	bool is_wall_collision(point point);
};

class game_player {
public:
	game_player();
	~game_player();
	int get_score(void);
	void update_score(void);
	play_state get_state(void);
	void set_state(const play_state st);

private:
	nvml::obj::p<int> score;
	nvml::obj::p<play_state> state;
};

class game_state {
public:
	game_state();
	~game_state();
	nvml::obj::persistent_ptr<game_board> get_board();
	nvml::obj::persistent_ptr<game_player> get_player();
	void init(void);
	void clean_pool(void);

private:
	nvml::obj::persistent_ptr<game_board> board;
	nvml::obj::persistent_ptr<game_player> player;
};

class game {
public:
	game(struct parameters *par);
	~game();
	int init(void);
	void init_colors(void);
	void process_step(void);
	int process_key(const int lastkey);
	inline bool is_stopped(void);
	void process_delay(void);
	void clear_screen(void);
	bool is_game_over(void);
	void game_over(void);
	void clear_prog(void);

private:
	nvml::obj::pool<game_state> state;
	int last_key;
	int delay;
	struct parameters *params;
	direction direction_key;

	void clean_pool(void);
	void set_direction_key(void);
	int parse_conf_create_dynamic_layout(void);
};

#endif /* PANACONDA_HPP */
