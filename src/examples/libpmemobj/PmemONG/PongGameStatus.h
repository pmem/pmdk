/*
 * PongGameStatus.h
 *
 *  Created on: 10 lip 2017
 *      Author: huber
 */

#ifndef PONGGAMESTATUS_H_
#define PONGGAMESTATUS_H_
#include "Paddle.h"
#include "Ball.h"
#include "GameConstants.h"
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/transaction.hpp>
#include "GameConstants.h"



class PongGameStatus {
public:
	PongGameStatus();
	~PongGameStatus();

	void startBall(float ballSpeed);
	void reset();
	void movePaddles();
	void lookForCollisions(bool increaseBallVelocity);
	void actualizeStatus();
	void simulation();

	float randomizeFloatValue(float min, float max);

	bool score();
	bool randomizeDirection();


	nvml::obj::persistent_ptr<Paddle> getPlayer1();
	nvml::obj::persistent_ptr<Paddle> getPlayer2();
	nvml::obj::persistent_ptr<Ball> getBall();


private:
	nvml::obj::persistent_ptr<Paddle> player1;
	nvml::obj::persistent_ptr<Paddle> player2;
	nvml::obj::persistent_ptr<Ball> ball;




};



#endif /* PONGGAMESTATUS_H_ */
