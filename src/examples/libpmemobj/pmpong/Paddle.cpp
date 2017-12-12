/*
 * Copyright 2017, Intel Corporation
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

#include "Paddle.hpp"
#include "Pool.hpp"

Pool *gamePoolP;

Paddle::Paddle(int x, int y)
{
	this->x = x;
	this->y = y;
	this->points = 0;
	init();
}

Paddle::~Paddle()
{
}

void
Paddle::moveUp(int velocity)
{
	if (!(this->y - velocity <
	      SCORE_VIEW_OFFSET + HORIZONAL_LINE_OFFSET + LINE_THICKNESS)) {
		setY(this->y - velocity);
	} else if (this->y - velocity <
		   SCORE_VIEW_OFFSET + HORIZONAL_LINE_OFFSET + LINE_THICKNESS) {
		setY(SCORE_VIEW_OFFSET + HORIZONAL_LINE_OFFSET +
		     LINE_THICKNESS);
	}
}

void
Paddle::moveDown(int velocity)
{
	if (!(this->y + PADDLE_HEIGHT + velocity >
	      WINDOW_HEIGHT - HORIZONAL_LINE_OFFSET - LINE_THICKNESS)) {
		setY(this->y + velocity);
	} else if (this->y + PADDLE_HEIGHT + velocity >
		   WINDOW_HEIGHT - HORIZONAL_LINE_OFFSET - LINE_THICKNESS) {
		setY(WINDOW_HEIGHT - HORIZONAL_LINE_OFFSET - PADDLE_HEIGHT);
	}
}

void
Paddle::addPoint()
{
	setPoints(points + 1);
}

void
Paddle::init()
{
	setY(WINDOW_HEIGHT / 2 - (int)getPaddleShape().getSize().y / 2);
}

void
Paddle::adjustPaddleYtoBall(Ball &ball)
{
	if (this->y > ball.getY())
		moveUp(PADDLE_VELOCITY_COMPUTER);
	if (this->y + getPaddleShape().getGlobalBounds().height -
		    ball.getBallShape().getRadius() * 4 <
	    ball.getY())
		moveDown(PADDLE_VELOCITY_COMPUTER);
}

void
Paddle::collisionWithBall(Ball &ball, bool increaseBallSpeed)
{
	if (ball.getBallShape().getGlobalBounds().intersects(
		    getPaddleShape().getGlobalBounds())) {
		ball.setVelocityX(ball.getVelocity()->x * (-1));
		if (increaseBallSpeed)
			ball.increaseVelocity();
	}
}

int
Paddle::getX()
{
	return this->x;
}

int
Paddle::getY()
{
	return this->y;
}

int
Paddle::getPoints()
{
	return this->points;
}

sf::RectangleShape
Paddle::getPaddleShape()
{
	sf::RectangleShape shapeToRet;
	shapeToRet.setSize(sf::Vector2f(PADDLE_WIDTH, PADDLE_HEIGHT));
	shapeToRet.setPosition(sf::Vector2f((float)this->x, (float)this->y));
	return shapeToRet;
}

void
Paddle::setPoints(int pointsArg)
{
	nvml::obj::transaction::exec_tx(
		gamePoolP->getGamePool()->getPoolToTransaction(),
		[&] { points = pointsArg; });
}

void
Paddle::setY(int yArg)
{
	nvml::obj::transaction::exec_tx(
		gamePoolP->getGamePool()->getPoolToTransaction(),
		[&] { y = yArg; });
}

void
Paddle::setX(int xArg)
{
	nvml::obj::transaction::exec_tx(
		gamePoolP->getGamePool()->getPoolToTransaction(),
		[&] { x = xArg; });
}
