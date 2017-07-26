/*
 * Ball.cpp
 *
 *  Created on: 10 lip 2017
 *      Author: huber
 */

#include "Ball.h"
#include "Pool.h"

Pool *gamePoolB;

Ball::Ball(int x, int y){
	this->x = x;
	this->y = y;
	velocity = nvml::obj::make_persistent<sf::Vector2f>();
	this->velocity->x = 0;
	this->velocity->y = 0;
}

Ball::~Ball() {
	nvml::obj::transaction::exec_tx(gamePoolB->getGamePool()->getPoolToTransaction(), [&]{
		nvml::obj::delete_persistent<sf::Vector2f>(velocity);
	});
}

void Ball::move(){
	setXY(this->x + (int)this->velocity->x, this->y + (int)this->velocity->y);
}

void Ball::collisionWithWindow(){
	if(this->y <= SCORE_VIEW_OFFSET + HORIZONAL_LINE_OFFSET || this->y + getBallShape().getRadius() * 2  >= WINDOW_HEIGHT - HORIZONAL_LINE_OFFSET){
		setVelocityY(velocity->y*-1);
	}
}

void Ball::increaseVelocity(){
	if(velocity->x < 0){
		setVelocityX(velocity->x - BALL_VELOCITY_INCREMENTING);
	}
	else{
		setVelocityX(velocity->x + BALL_VELOCITY_INCREMENTING);
	}
	if(velocity->y < 0 ){
		setVelocityY(velocity->y - BALL_VELOCITY_INCREMENTING);

	}
	else{
		setVelocityY(velocity->y + BALL_VELOCITY_INCREMENTING);
	}
}

void Ball::setX(int xArg){
	nvml::obj::transaction::exec_tx(gamePoolB->getGamePool()->getPoolToTransaction(), [&]{
		x = xArg;
	});
}

void Ball::setY(int yArg){
	nvml::obj::transaction::exec_tx(gamePoolB->getGamePool()->getPoolToTransaction(), [&]{
		y = yArg;
	});
}


void Ball::setVelocityX(float xArg){
	nvml::obj::transaction::exec_tx(gamePoolB->getGamePool()->getPoolToTransaction(), [&]{
		velocity->x = xArg;
	});

}

void Ball::setVelocityY(float yArg){
	nvml::obj::transaction::exec_tx(gamePoolB->getGamePool()->getPoolToTransaction(), [&]{
		velocity->y = yArg;
	});
}

void Ball::setXY(int xArg, int yArg){
	nvml::obj::transaction::exec_tx(gamePoolB->getGamePool()->getPoolToTransaction(), [&]{
		x = xArg;
		y = yArg;
	});
}

int Ball::getX(){
	return this->x;
}

int Ball::getY(){
	return this->y;
}

nvml::obj::persistent_ptr<sf::Vector2f> Ball::getVelocity(){
	return this->velocity;
}

void Ball::init(){
	setXY(WINDOW_WIDTH / 2, WINDOW_HEIGHT / 2);
	setVelocityX(0);
	setVelocityY(0);
}

sf::CircleShape Ball::getBallShape(){
	sf::CircleShape shapeToRet;
	shapeToRet.setRadius(BALL_SIZE);
	shapeToRet.setPosition(sf::Vector2f(this->x, this->y));
	return shapeToRet;
}
