/*
 * PongGameStatus.cpp
 *
 *  Created on: 10 lip 2017
 *      Author: huber
 */

#include "PongGameStatus.h"
#include "Pool.h"

Pool *gamePool;


PongGameStatus::PongGameStatus(){
	player1 = nvml::obj::make_persistent<Paddle>(VERTICAL_LINE_OFFSET + LINE_THICKNESS, WINDOW_HEIGHT/2);
	player2 = nvml::obj::make_persistent<Paddle>(WINDOW_WIDTH - VERTICAL_LINE_OFFSET - PADDLE_WIDTH, WINDOW_HEIGHT/2);
	ball = nvml::obj::make_persistent<Ball>(WINDOW_WIDTH/2, WINDOW_HEIGHT/2);
	menuItem = 0;
	isGameToResume = false;
	actualGameState = game_state::MENU;
}

PongGameStatus::~PongGameStatus() {
	nvml::obj::transaction::exec_tx(gamePool->getGamePool()->getPoolToTransaction(), [&]{
		nvml::obj::delete_persistent<Paddle>(player1);
		nvml::obj::delete_persistent<Paddle>(player2);
		nvml::obj::delete_persistent<Ball>(ball);
	});
}


void PongGameStatus::startBall(float ballSpeed){
	if(ball->getVelocity()->x == 0 && ball->getVelocity()->y == 0){
		float x = randomizeFloatValue(1.5, 2.0);
		ball->setVelocityX(randomizeDirection() ? ballSpeed : -ballSpeed);
		ball->setVelocityY(randomizeDirection() ? x : -1 * x);
	}
}

void PongGameStatus::reset(){
	ball->init();
	player1->init();
	player2->init();
}

bool PongGameStatus::score(){
	if(ball->getBallShape().getPosition().x > WINDOW_WIDTH - VERTICAL_LINE_OFFSET + LINE_THICKNESS - ball->getBallShape().getRadius() * 2){
		player1->addPoint();
		reset();
		return true;
	}
	if(ball->getBallShape().getPosition().x < VERTICAL_LINE_OFFSET - LINE_THICKNESS){
		player2->addPoint();
		reset();
		return true;
	}
	return false;
}

void PongGameStatus::movePaddles(){
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::W)){
		player1->moveUp(PADDLE_VELOCITY_PLAYER);
	}
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::S)){
		player1->moveDown(PADDLE_VELOCITY_PLAYER);
	}
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Up)){
		player2->moveUp(PADDLE_VELOCITY_PLAYER);
	}
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Down)){
		player2->moveDown(PADDLE_VELOCITY_PLAYER);
	}
}

void PongGameStatus::lookForCollisions(bool increaseBallVelocity){
	player1->collisionWithBall(*ball, increaseBallVelocity);
	player2->collisionWithBall(*ball, increaseBallVelocity);
	ball->collisionWithWindow();
}

void PongGameStatus::actualizeStatus(){
	ball->move();
}

nvml::obj::persistent_ptr<Paddle> PongGameStatus::getPlayer1(){
	return this->player1;
}

nvml::obj::persistent_ptr<Paddle> PongGameStatus::getPlayer2(){
	return this->player2;
}

nvml::obj::persistent_ptr<Ball> PongGameStatus::getBall(){
	return this->ball;
}


void PongGameStatus::simulate(){
	if(ball->getVelocity()->x > 0)
		player2->adjustPaddleYtoBall(*ball);
	if(ball->getVelocity()->x < 0)
		player1->adjustPaddleYtoBall(*ball);
}

bool PongGameStatus::checkIfAnyPlayerWon(){
	if(getPlayer1()->getPoints() == POINTS_TO_WIN || getPlayer2()->getPoints() == POINTS_TO_WIN)
		return true;
	return false;
}

bool PongGameStatus::randomizeDirection()
{
	 static auto gen = std::bind(std::uniform_int_distribution<>(0,1),std::default_random_engine());
	 return gen();
}

float PongGameStatus::randomizeFloatValue(float min, float max){
	return (min + 1) + (((float) rand()) / (float) RAND_MAX) * (max - (min + 1));
}

bool PongGameStatus::getIsGameToResume(){
	return this->isGameToResume;
}

int PongGameStatus::getMenuItem(){
	return this->menuItem;
}

void PongGameStatus::setIsGameToResume(bool isGameToRes){
	nvml::obj::transaction::exec_tx(gamePool->getGamePool()->getPoolToTransaction(), [&]{
		isGameToResume = isGameToRes;
	});
}

void PongGameStatus::setMenuItem(int numb){
	nvml::obj::transaction::exec_tx(gamePool->getGamePool()->getPoolToTransaction(), [&]{
		this->menuItem = numb;
	});
}

void PongGameStatus::setGameState(game_state state){
	nvml::obj::transaction::exec_tx(gamePool->getGamePool()->getPoolToTransaction(), [&]{
		this->actualGameState = state;
	});
}

game_state PongGameStatus::getGameState(){
	return this->actualGameState;
}
