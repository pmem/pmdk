/*
 * Paddle.h
 *
 *  Created on: 10 lip 2017
 *      Author: huber
 */

#ifndef PADDLE_H_
#define PADDLE_H_

#include <SFML/Graphics.hpp>
#include <libpmemobj++/p.hpp>
#include <libpmemobj++/persistent_ptr.hpp>
#include <libpmemobj++/pool.hpp>
#include "GameConstants.h"
#include "Ball.h"


class Paddle {

public:
	Paddle();
	Paddle(int x, int y);
	~Paddle();

	void moveUp(int velocity);
	void moveDown(int velocity);
	void addPoint();
	void init();
	void adjustPaddleYtoBall(Ball &ball);
	void collisionWithBall(Ball &ball, bool increaseBallSpeed);

	int getX();
	int getY();
	int getPoints();

	sf::RectangleShape getPaddleShape();

private:
	nvml::obj::p<int> y;
	nvml::obj::p<int> x;
	nvml::obj::p<int> points;

	void setY(int yArg);
	void setX(int xArg);
	void setPoints(int pointsArg);
};

#endif /* PADDLE_H_ */
