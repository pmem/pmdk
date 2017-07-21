/*
 * Game.h
 *
 *  Created on: 12 lip 2017
 *      Author: huber
 */

#ifndef GAME_H_
#define GAME_H_
#include "GameConstants.h"
#include "PongGameStatus.h"
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/transaction.hpp>
#include <libpmemobj++/make_persistent.hpp>


enum game_state {MATCH, MENU, GAME_OVER, COMPUTER_PLAY};

class Game {
public:
	Game();
	~Game();

	void gameLooping(bool isOnlySimulation = false);

private:
	nvml::obj::p<game_state> actualGameState;
	nvml::obj::p<int> menuItem;
	nvml::obj::p<bool> isGameToResume;
	nvml::obj::persistent_ptr<PongGameStatus> board;


	void menuView(sf::RenderWindow *gameWindow, sf::Font &font);
	void gameView(sf::RenderWindow *gameWindow, sf::Font &font, bool isSimulation);
	void gameOverView(sf::RenderWindow *gameWindow, sf::Font &font);
	void setGameState(game_state state);
	void setMenuItem(int numb);
	void setIsGameToResume(bool isGameToRes);
	void setNewGame();


};

#endif /* GAME_H_ */
