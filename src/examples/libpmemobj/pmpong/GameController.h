/*
 * Game.h
 *
 *  Created on: 12 lip 2017
 *      Author: huber
 */

#ifndef GAMECONTROLLER_H_
#define GAMECONTROLLER_H_
#include "GameConstants.h"
#include "PongGameStatus.h"
#include "GameOverView.h"
#include "GameView.h"
#include "MenuView.h"
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/pool.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/transaction.hpp>
#include <libpmemobj++/make_persistent.hpp>


class GameController {
public:
	GameController();
	~GameController();

	void gameLooping(bool isOnlySimulation = false);

private:
	nvml::obj::persistent_ptr<PongGameStatus> gameStatus;


	void menu(sf::RenderWindow *gameWindow, View *view);
	void gameMatch(sf::RenderWindow *gameWindow, View *view);
	void gameOver(sf::RenderWindow *gameWindow, View *view);
	void gameMatchSimulation(sf::RenderWindow *gameWindow, View *view, bool isOnlySimulation);

	void resetGameStatus();


};

#endif /* GAMECONTROLLER_H_ */
