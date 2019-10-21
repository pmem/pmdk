/*
 * Copyright 2015-2019, Intel Corporation
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
 * pminvaders2.c -- PMEM-based clone of space invaders (version 2.0)
 *
 * RULES:
 *   +1 point for each alien destroyed (72 per level)
 *   -100 points and move to a lower level when killed
 */

#include <stddef.h>
#ifdef __FreeBSD__
#include <ncurses/ncurses.h>	/* Need pkg, not system, version */
#else
#include <ncurses.h>
#endif
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <sys/stat.h>

#include <libpmem.h>
#include <libpmemobj.h>

/*
 * Layout definition
 */
POBJ_LAYOUT_BEGIN(pminvaders2);
POBJ_LAYOUT_ROOT(pminvaders2, struct root);
POBJ_LAYOUT_TOID(pminvaders2, struct state);
POBJ_LAYOUT_TOID(pminvaders2, struct alien);
POBJ_LAYOUT_TOID(pminvaders2, struct player);
POBJ_LAYOUT_TOID(pminvaders2, struct bullet);
POBJ_LAYOUT_TOID(pminvaders2, struct star);
POBJ_LAYOUT_END(pminvaders2);

#define POOL_SIZE	(100 * 1024 * 1024) /* 100 megabytes */

#define GAME_WIDTH	50
#define GAME_HEIGHT	25

#define ALIENS_ROW	4
#define ALIENS_COL	18

#define RRAND(min, max)	(rand() % ((max) - (min) + 1) + (min))

#define STEP 50

#define PLAYER_Y (GAME_HEIGHT - 1)

#define MAX_GSTATE_TIMER 10000
#define MIN_GSTATE_TIMER 5000

#define MAX_ALIEN_TIMER	1000
#define MAX_PLAYER_TIMER 1000
#define MAX_BULLET_TIMER 500
#define MAX_STAR1_TIMER 200
#define MAX_STAR2_TIMER 100

enum game_event {
	EVENT_NOP,
	EVENT_BOUNCE,
	EVENT_PLAYER_KILLED,
	EVENT_ALIENS_KILLED
};

enum colors {
	C_UNKNOWN,
	C_PLAYER,
	C_ALIEN,
	C_BULLET,
	C_STAR,
	C_INTRO
};

struct state {
	unsigned timer;
	int score;
	unsigned high_score;
	int level;
	int new_level;
	int dx;
	int dy;
};

struct player {
	unsigned x;
	unsigned timer;
};

struct alien {
	unsigned x;
	unsigned y;
	TOID(struct alien) prev;
	TOID(struct alien) next;
};

struct star {
	unsigned x;
	unsigned y;
	int c;
	unsigned timer;
	TOID(struct star) prev;
	TOID(struct star) next;
};

struct bullet {
	unsigned x;
	unsigned y;
	unsigned timer;
	TOID(struct bullet) prev;
	TOID(struct bullet) next;
};

struct root {
	TOID(struct state) state;
	TOID(struct player) player;
	TOID(struct alien) aliens;
	TOID(struct bullet) bullets;
	TOID(struct star) stars;
};

/*
 * draw_star -- draw a star
 */
static void
draw_star(const struct star *s)
{
	mvaddch(s->y, s->x, s->c | COLOR_PAIR(C_STAR));
}

/*
 * draw_alien -- draw an alien
 */
static void
draw_alien(const struct alien *a)
{
	mvaddch(a->y, a->x, ACS_DIAMOND | COLOR_PAIR(C_ALIEN));
}

/*
 * draw_player -- draw a player
 */
static void
draw_player(const struct player *p)
{
	mvaddch(PLAYER_Y, p->x, ACS_DIAMOND | COLOR_PAIR(C_PLAYER));
}

/*
 * draw_bullet -- draw a bullet
 */
static void
draw_bullet(const struct bullet *b)
{
	mvaddch(b->y, b->x, ACS_BULLET | COLOR_PAIR(C_BULLET));
}

/*
 * draw_score -- draw the game score and the global highest score
 */
static void
draw_score(const struct state *s)
{
	mvprintw(1, 1, "Level: %u    Score: %u | %u\n",
		s->level, s->score, s->high_score);
}

/*
 * draw_title -- draw the game title and menu
 */
static void
draw_title(void)
{
	int x = (GAME_WIDTH - 40) / 2;
	int y = GAME_HEIGHT / 2 - 2;

	attron(COLOR_PAIR(C_INTRO));

	mvprintw(y + 0, x, "#### #   # ### #   # #   #     ###   ###");
	mvprintw(y + 1, x, "#  # ## ##  #  ##  # #   #       #   # #");
	mvprintw(y + 2, x, "#### # # #  #  # # #  # #      ###   # #");
	mvprintw(y + 3, x, "#    # # #  #  #  ##  # #      #     # #");
	mvprintw(y + 4, x, "#    #   # ### #   #   #       ### # ###");

	attroff(COLOR_PAIR(C_INTRO));

	mvprintw(y + 6, x, "      Press 'space' to resume           ");
	mvprintw(y + 7, x, "      Press 'q' to quit                 ");
}

/*
 * draw_border -- draw a frame around the map
 */
static void
draw_border(void)
{
	for (int x = 0; x <= GAME_WIDTH; ++x) {
		mvaddch(0, x, ACS_HLINE);
		mvaddch(GAME_HEIGHT, x, ACS_HLINE);
	}

	for (int y = 0; y <= GAME_HEIGHT; ++y) {
		mvaddch(y, 0, ACS_VLINE);
		mvaddch(y, GAME_WIDTH, ACS_VLINE);
	}

	mvaddch(0, 0, ACS_ULCORNER);
	mvaddch(GAME_HEIGHT, 0, ACS_LLCORNER);
	mvaddch(0, GAME_WIDTH, ACS_URCORNER);
	mvaddch(GAME_HEIGHT, GAME_WIDTH, ACS_LRCORNER);
}

/*
 * timer_tick -- very simple persistent timer
 */
static int
timer_tick(unsigned *timer)
{
	return *timer == 0 || ((*timer)--) == 0;
}

/*
 * create_star -- create a single star at random position
 */
static TOID(struct star)
create_star(unsigned x, unsigned y, TOID(struct star) next)
{
	TOID(struct star) s = TX_ZNEW(struct star);
	struct star *sp = D_RW(s);

	sp->x = x;
	sp->y = y;
	sp->c = rand() % 2 ? '*' : '.';
	sp->timer = sp->c == '.' ? MAX_STAR1_TIMER : MAX_STAR2_TIMER;

	sp->prev = TOID_NULL(struct star);
	sp->next = next;
	if (!TOID_IS_NULL(next))
		D_RW(next)->prev = s;

	return s;
}

/*
 * create_stars -- create a new set of stars at random positions
 */
static void
create_stars(TOID(struct root) r)
{
	for (int x = 1; x < GAME_WIDTH; x++) {
		if (rand() % 100 < 4)
			D_RW(r)->stars = create_star(x, 1, D_RW(r)->stars);
	}
}

/*
 * process_stars -- process creation and movement of the stars
 */
static void
process_stars(PMEMobjpool *pop, TOID(struct root) r)
{
	int new_line = 0;

	TX_BEGIN(pop) {
		TOID(struct star) s = D_RO(r)->stars;
		while (!TOID_IS_NULL(s)) {
			TX_ADD(s);
			struct star *sptr = D_RW(s);

			TOID(struct star) sp = sptr->prev;
			TOID(struct star) sn = sptr->next;

			if (timer_tick(&sptr->timer)) {
				sptr->timer = sptr->c == '.'
					? MAX_STAR1_TIMER : MAX_STAR2_TIMER;
				sptr->y++;
				if (sptr->c == '.')
					new_line = 1;
			}

			draw_star(sptr);

			if (sptr->y >= GAME_HEIGHT) {
				if (!TOID_IS_NULL(sp)) {
					TX_ADD(sp);
					D_RW(sp)->next = sn;
				} else {
					TX_ADD(r);
					D_RW(r)->stars = sn;
				}
				if (!TOID_IS_NULL(sn)) {
					TX_ADD(sn);
					D_RW(sn)->prev = sp;
				}
				TX_FREE(s);
			}

			s = sn;
		}

		if (new_line)
			create_stars(r);
	} TX_END;
}

/*
 * create_alien -- create an alien at given position
 */
static TOID(struct alien)
create_alien(unsigned x, unsigned y, TOID(struct alien) next)
{
	TOID(struct alien) a = TX_ZNEW(struct alien);
	struct alien *ap = D_RW(a);

	ap->x = x;
	ap->y = y;

	ap->prev = TOID_NULL(struct alien);
	ap->next = next;
	if (!TOID_IS_NULL(next))
		D_RW(next)->prev = a;

	return a;
}

/*
 * create_aliens -- create new set of aliens
 */
static void
create_aliens(TOID(struct root) r)
{
	for (int x = 0; x < ALIENS_COL; x++) {
		for (int y = 0; y < ALIENS_ROW; y++) {
			unsigned pos_x =
				(GAME_WIDTH / 2) - (ALIENS_COL) + (x * 2);
			unsigned pos_y = y + 3;
			D_RW(r)->aliens =
				create_alien(pos_x, pos_y, D_RW(r)->aliens);
		}
	}
}

/*
 * remove_aliens -- remove all the aliens from the map
 */
static void
remove_aliens(TOID(struct alien) *ah)
{
	while (!TOID_IS_NULL(*ah)) {
		TOID(struct alien) an = D_RW(*ah)->next;
		TX_FREE(*ah);
		*ah = an;
	}
}

/*
 * move_aliens -- process movement of the aliens
 */
static int
move_aliens(PMEMobjpool *pop, TOID(struct root) r, int dx, int dy)
{
	int ret = EVENT_NOP;
	int cnt = 0;

	TOID(struct alien) a = D_RO(r)->aliens;
	while (!TOID_IS_NULL(a)) {
		TX_ADD(a);
		struct alien *ap = D_RW(a);

		cnt++;
		if (dy)
			ap->y += dy;
		else
			ap->x += dx;

		if (ap->y >= PLAYER_Y)
			ret = EVENT_PLAYER_KILLED;
		else if (dy == 0 && (ap->x >= GAME_WIDTH - 2 || ap->x <= 2))
			ret = EVENT_BOUNCE;

		a = ap->next;
	}

	if (cnt == 0)
		ret = EVENT_ALIENS_KILLED; /* all killed */

	return ret;
}

/*
 * create_player -- spawn the player in the middle of the map
 */
static TOID(struct player)
create_player(void)
{
	TOID(struct player) p = TX_ZNEW(struct player);
	struct player *pp = D_RW(p);

	pp->x = GAME_WIDTH / 2;
	pp->timer = 1;

	return p;
}

/*
 * create_bullet -- spawn the bullet at the position of the player
 */
static TOID(struct bullet)
create_bullet(unsigned x, TOID(struct bullet) next)
{
	TOID(struct bullet) b = TX_ZNEW(struct bullet);
	struct bullet *bp = D_RW(b);

	bp->x = x;
	bp->y = PLAYER_Y - 1;
	bp->timer = 1;
	bp->prev = TOID_NULL(struct bullet);
	bp->next = next;
	if (!TOID_IS_NULL(next))
		D_RW(next)->prev = b;

	return b;
}

/*
 * create_state -- create game state
 */
static TOID(struct state)
create_state(void)
{
	TOID(struct state) s = TX_ZNEW(struct state);
	struct state *sp = D_RW(s);

	sp->timer = 1;
	sp->score = 0;
	sp->high_score = 0;
	sp->level = 0;
	sp->new_level = 1;
	sp->dx = 1;
	sp->dy = 0;

	return s;
}

/*
 * new_level -- prepare map for the new game level
 */
static void
new_level(PMEMobjpool *pop, TOID(struct root) r)
{
	TX_BEGIN(pop) {
		TX_ADD(r);
		struct root *rp = D_RW(r);

		remove_aliens(&rp->aliens);
		create_aliens(r);

		TX_ADD(rp->state);
		struct state *sp = D_RW(rp->state);

		if (sp->new_level > 0 || sp->level > 1)
			sp->level += sp->new_level;

		sp->new_level = 0;
		sp->dx = 1;
		sp->dy = 0;
		sp->timer = MAX_ALIEN_TIMER -
				100 * (sp->level - 1);
	} TX_END;
}

/*
 * update_score -- change player score and global high score
 */
static void
update_score(struct state *sp, int m)
{
	if (m < 0 && sp->score == 0)
		return;

	sp->score += m;
	if (sp->score < 0)
		sp->score = 0;
	if (sp->score > sp->high_score)
		sp->high_score = sp->score;
}

/*
 * process_aliens -- process movement of the aliens and game events
 */
static void
process_aliens(PMEMobjpool *pop, TOID(struct root) r)
{
	TX_BEGIN(pop) {
		TOID(struct state) s = D_RO(r)->state;
		TX_ADD(s);
		struct state *sp = D_RW(s);

		if (timer_tick(&sp->timer)) {
			sp->timer = MAX_ALIEN_TIMER - 50 * (sp->level - 1);
			switch (move_aliens(pop, r, sp->dx, sp->dy)) {
			case EVENT_ALIENS_KILLED:
				/* all aliens killed */
				sp->new_level = 1;
				break;

			case EVENT_PLAYER_KILLED:
				/* player killed */
				flash();
				beep();
				sp->new_level = -1;
				update_score(sp, -100);
				break;

			case EVENT_BOUNCE:
				/* move down + change direction */
				sp->dy = 1;
				sp->dx = -sp->dx;
				break;

			case EVENT_NOP:
				/* do nothing */
				sp->dy = 0;
				break;
			}
		}
	} TX_END;

	TOID(struct alien) a = D_RO(r)->aliens;
	while (!TOID_IS_NULL(a)) {
		const struct alien *ap = D_RO(a);
		draw_alien(ap);
		a = ap->next;
	}
}

/*
 * process_collision -- search for any aliens on the position of the bullet
 */
static int
process_collision(PMEMobjpool *pop, TOID(struct root) r,
	struct state *sp, const struct bullet *bp)
{
	int ret = 0;

	TX_BEGIN(pop) {
		TOID(struct alien) a = D_RO(r)->aliens;
		while (!TOID_IS_NULL(a)) {
			struct alien *aptr = D_RW(a);

			TOID(struct alien) ap = aptr->prev;
			TOID(struct alien) an = aptr->next;

			if (bp->x == aptr->x && bp->y == aptr->y) {
				update_score(sp, 1);
				if (!TOID_IS_NULL(ap)) {
					TX_ADD(ap);
					D_RW(ap)->next = an;
				} else {
					TX_ADD(r);
					D_RW(r)->aliens = an;
				}
				if (!TOID_IS_NULL(an)) {
					TX_ADD(an);
					D_RW(an)->prev = ap;
				}
				TX_FREE(a);
				ret = 1;
				break;
			}

			a = an;
		}
	} TX_END;

	return ret;
}

/*
 * process_bullets -- process bullets movement and collision
 */
static void
process_bullets(PMEMobjpool *pop, TOID(struct root) r, struct state *sp)
{
	TX_BEGIN(pop) {
		TOID(struct bullet) b = D_RO(r)->bullets;
		while (!TOID_IS_NULL(b)) {
			TX_ADD(b);
			struct bullet *bptr = D_RW(b);

			TOID(struct bullet) bp = bptr->prev;
			TOID(struct bullet) bn = bptr->next;

			if (timer_tick(&bptr->timer)) {
				bptr->timer = MAX_BULLET_TIMER;
				bptr->y--;
			}

			draw_bullet(bptr);

			if (bptr->y <= 0 ||
			    process_collision(pop, r, sp, bptr)) {
				if (!TOID_IS_NULL(bp)) {
					TX_ADD(bp);
					D_RW(bp)->next = bn;
				} else {
					TX_ADD(r);
					D_RW(r)->bullets = bn;
				}
				if (!TOID_IS_NULL(bn)) {
					TX_ADD(bn);
					D_RW(bn)->prev = bp;
				}
				TX_FREE(b);
			}

			b = bn;
		}
	} TX_END;
}

/*
 * process_player -- handle player actions
 */
static void
process_player(PMEMobjpool *pop, TOID(struct root) r, int input)
{
	TOID(struct player) p = D_RO(r)->player;

	TX_BEGIN(pop) {
		TX_ADD(r);
		TX_ADD(p);

		struct player *pp = D_RW(p);

		timer_tick(&pp->timer);

		if (input == KEY_LEFT || input == 'o') {
			unsigned dstx = pp->x - 1;
			if (dstx != 0)
				pp->x = dstx;
		} else if (input == KEY_RIGHT || input == 'p') {
			unsigned dstx = pp->x + 1;
			if (dstx != GAME_WIDTH)
				pp->x = dstx;
		} else if (input == ' ' && pp->timer == 0) {
			pp->timer = MAX_PLAYER_TIMER;
			D_RW(r)->bullets =
				create_bullet(pp->x, D_RW(r)->bullets);
		}
	} TX_END;

	draw_player(D_RO(p));
}

/*
 * game_init -- create and initialize game state and the player
 */
static TOID(struct root)
game_init(PMEMobjpool *pop)
{
	TOID(struct root) r = POBJ_ROOT(pop, struct root);

	TX_BEGIN(pop) {
		TX_ADD(r);
		struct root *rp = D_RW(r);

		if (TOID_IS_NULL(rp->state))
			rp->state = create_state();
		if (TOID_IS_NULL(rp->player))
			rp->player = create_player();
	} TX_END;

	return r;
}

/*
 * game_loop -- process drawing and logic of the game
 */
static int
game_loop(PMEMobjpool *pop, TOID(struct root) r)
{
	int input = getch();

	TOID(struct state) s = D_RO(r)->state;
	struct state *sp = D_RW(s);

	erase();
	draw_score(sp);
	draw_border();

	TX_BEGIN(pop) {
		TX_ADD(r);
		TX_ADD(s);

		if (sp->new_level != 0)
			new_level(pop, r);

		process_aliens(pop, r);
		process_bullets(pop, r, sp);
		process_player(pop, r, input);
	} TX_END;

	usleep(STEP);
	refresh();

	if (input == 'q')
		return -1;
	else
		return 0;
}

/*
 * intro_loop -- process drawing of the intro animation
 */
static int
intro_loop(PMEMobjpool *pop, TOID(struct root) r)
{
	int input = getch();

	erase();
	draw_border();

	TX_BEGIN(pop) {
		TX_ADD(r);
		struct root *rp = D_RW(r);

		if (TOID_IS_NULL(rp->stars))
			create_stars(r);

		process_stars(pop, r);
	} TX_END;

	draw_title();

	usleep(STEP);
	refresh();

	switch (input) {
		case ' ':
			return 1;
		case 'q':
			return -1;
		default:
			return 0;
	}
}

int
main(int argc, char *argv[])
{
	static PMEMobjpool *pop;
	int in;

	if (argc != 2)
		exit(1);

	srand(time(NULL));

	if (access(argv[1], F_OK)) {
		if ((pop = pmemobj_create(argv[1],
				POBJ_LAYOUT_NAME(pminvaders2),
				POOL_SIZE, S_IRUSR | S_IWUSR)) == NULL) {
			fprintf(stderr, "%s", pmemobj_errormsg());
			exit(1);
		}
	} else {
		if ((pop = pmemobj_open(argv[1],
				POBJ_LAYOUT_NAME(pminvaders2))) == NULL) {
			fprintf(stderr, "%s", pmemobj_errormsg());
			exit(1);
		}
	}

	initscr();
	start_color();
	init_pair(C_PLAYER, COLOR_GREEN, COLOR_BLACK);
	init_pair(C_ALIEN, COLOR_RED, COLOR_BLACK);
	init_pair(C_BULLET, COLOR_YELLOW, COLOR_BLACK);
	init_pair(C_STAR, COLOR_WHITE, COLOR_BLACK);
	init_pair(C_INTRO, COLOR_BLUE, COLOR_BLACK);
	nodelay(stdscr, true);
	curs_set(0);
	keypad(stdscr, true);

	TOID(struct root) r = game_init(pop);

	while ((in = intro_loop(pop, r)) == 0)
		;

	if (in == -1)
		goto end;

	while ((in = game_loop(pop, r)) == 0)
		;

end:
	endwin();

	pmemobj_close(pop);

	return 0;
}
