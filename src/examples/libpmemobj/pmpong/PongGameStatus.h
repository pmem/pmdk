/*
 * PongGameStatus.h
 *
 *  Created on: 10 lip 2017
 *      Author: huber
 */

#ifndef PONGGAMESTATUS_H_
#define PONGGAMESTATUS_H_
#include <SFML/Graphics.hpp>
#include "GameConstants.h"
#include "Paddle.h"
#include "Ball.h"
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/transaction.hpp>
#include "GameConstants.h"

enum game_state {MATCH, MENU, GAME_OVER, SIMULATE};

class PongGameStatus {
public:
	PongGameStatus();
	~PongGameStatus();

	void startBall(float ballSpeed);
	void reset();
	void movePaddles();
	void lookForCollisions(bool increaseBallVelocity);
	void actualizeStatus();
	void simulate();
	void setMenuItem(int numb);
	void setIsGameToResume(bool isGameToRes);
	void setGameState(game_state state);


	int getMenuItem();


	float randomizeFloatValue(float min, float max);

	bool score();
	bool checkIfAnyPlayerWon();
	bool randomizeDirection();
	bool getIsGameToResume();

	game_state getGameState();


	nvml::obj::persistent_ptr<Paddle> getPlayer1();
	nvml::obj::persistent_ptr<Paddle> getPlayer2();
	nvml::obj::persistent_ptr<Ball> getBall();


private:
	nvml::obj::persistent_ptr<Paddle> player1;
	nvml::obj::persistent_ptr<Paddle> player2;
	nvml::obj::persistent_ptr<Ball> ball;

	nvml::obj::p<int> menuItem;
	nvml::obj::p<bool> isGameToResume;
	nvml::obj::p<game_state> actualGameState;

};



#endif /* PONGGAMESTATUS_H_ */
