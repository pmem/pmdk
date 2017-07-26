/*
 * Ball.h
 *
 *  Created on: 10 lip 2017
 *      Author: huber
 */

#ifndef BALL_H_
#define BALL_H_
#include <SFML/Graphics.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/transaction.hpp>
#include "GameConstants.h"




class Ball {
public:
	Ball(int x, int y);
	~Ball();

	void move();
	void increaseVelocity();
	void setX(int xArg);
	void setY(int yArg);
	void setXY(int xArg, int yArg);
	void setVelocityX(float xArg);
	void setVelocityY(float yArg);
	void init();
	void collisionWithWindow();

	nvml::obj::persistent_ptr<sf::Vector2f> getVelocity();

	sf::CircleShape getBallShape();

	int getX();
	int getY();

private:
	nvml::obj::p<int> x;
	nvml::obj::p<int> y;
	nvml::obj::persistent_ptr<sf::Vector2f> velocity;
};

#endif /* BALL_H_ */
