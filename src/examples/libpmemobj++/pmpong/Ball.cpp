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

#include "Ball.hpp"
#include "Pool.hpp"

Pool *gamePoolB;

Ball::Ball(int x, int y)
{
	this->x = x;
	this->y = y;
	velocity = pmem::obj::make_persistent<sf::Vector2f>();
	this->velocity->x = 0;
	this->velocity->y = 0;
}

Ball::~Ball()
{
	pmem::obj::transaction::exec_tx(
		gamePoolB->getGamePool()->getPoolToTransaction(),
		[&] { pmem::obj::delete_persistent<sf::Vector2f>(velocity); });
}

void
Ball::move()
{
	setXY(this->x + (int)this->velocity->x,
	      this->y + (int)this->velocity->y);
}

void
Ball::collisionWithWindow()
{
	if (this->y <= SCORE_VIEW_OFFSET + HORIZONAL_LINE_OFFSET ||
	    this->y + getBallShape().getRadius() * 2 >=
		    WINDOW_HEIGHT - HORIZONAL_LINE_OFFSET) {
		setVelocityY(velocity->y * -1);
	}
}

void
Ball::increaseVelocity()
{
	if (velocity->x < 0) {
		setVelocityX(velocity->x - BALL_VELOCITY_INCREMENTING);
	} else {
		setVelocityX(velocity->x + BALL_VELOCITY_INCREMENTING);
	}
	if (velocity->y < 0) {
		setVelocityY(velocity->y - BALL_VELOCITY_INCREMENTING);

	} else {
		setVelocityY(velocity->y + BALL_VELOCITY_INCREMENTING);
	}
}

void
Ball::setX(int xArg)
{
	pmem::obj::transaction::exec_tx(
		gamePoolB->getGamePool()->getPoolToTransaction(),
		[&] { x = xArg; });
}

void
Ball::setY(int yArg)
{
	pmem::obj::transaction::exec_tx(
		gamePoolB->getGamePool()->getPoolToTransaction(),
		[&] { y = yArg; });
}

void
Ball::setVelocityX(float xArg)
{
	pmem::obj::transaction::exec_tx(
		gamePoolB->getGamePool()->getPoolToTransaction(),
		[&] { velocity->x = xArg; });
}

void
Ball::setVelocityY(float yArg)
{
	pmem::obj::transaction::exec_tx(
		gamePoolB->getGamePool()->getPoolToTransaction(),
		[&] { velocity->y = yArg; });
}

void
Ball::setXY(int xArg, int yArg)
{
	pmem::obj::transaction::exec_tx(
		gamePoolB->getGamePool()->getPoolToTransaction(), [&] {
			x = xArg;
			y = yArg;
		});
}

void
Ball::init()
{
	setXY(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	setVelocityX(0);
	setVelocityY(0);
}

int
Ball::getX()
{
	return this->x;
}

int
Ball::getY()
{
	return this->y;
}

pmem::obj::persistent_ptr<sf::Vector2f>
Ball::getVelocity()
{
	return this->velocity;
}

sf::CircleShape
Ball::getBallShape()
{
	sf::CircleShape shapeToRet;
	shapeToRet.setRadius(BALL_SIZE);
	shapeToRet.setPosition(sf::Vector2f((float)this->x, (float)this->y));
	return shapeToRet;
}
