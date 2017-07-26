/*
 * GameOverView.cpp
 *
 *  Created on: 25 lip 2017
 *      Author: huber
 */

#include "GameOverView.h"

GameOverView::GameOverView(sf::Font &font) {
	gameOver.setString("GAME OVER");
	entContinue.setString("press ENTER to continue");
	playerWinner.setString("");
	gameOver.setFont(font);
	playerWinner.setFont(font);
	entContinue.setFont(font);
	gameOver.setCharacterSize(GAMEOVER_FONTSIZE);
	playerWinner.setCharacterSize(MENUVIEW_ITEMS_FONTSIZE);
	entContinue.setCharacterSize(MENUVIEW_ITEMS_FONTSIZE);
	gameOver.setPosition(WINDOW_WIDTH / 2 - gameOver.getGlobalBounds().width / 2, 0);
	playerWinner.setPosition(WINDOW_WIDTH / 2 - playerWinner.getGlobalBounds().width / 2, GAMOVERVIEW_OFFSET * 2);
	entContinue.setPosition(WINDOW_WIDTH / 2 - entContinue.getGlobalBounds().width / 2, WINDOW_HEIGHT - GAMOVERVIEW_OFFSET);
	gameOver.setFillColor(sf::Color::Red);
	playerWinner.setFillColor(sf::Color::Green);
	entContinue.setFillColor(sf::Color::White);
}

GameOverView::~GameOverView() {}

void GameOverView::prepareView(PongGameStatus &gameStatus){
	if(gameStatus.getPlayer1()->getPoints() == POINTS_TO_WIN)
		playerWinner.setString("LEFT PLAYER WON!");
	else
		playerWinner.setString("RIGHT PLAYER WON!");
}

void GameOverView::displayView(sf::RenderWindow *gameWindow){
	gameWindow->clear();
	gameWindow->draw(gameOver);
	gameWindow->draw(playerWinner);
	gameWindow->draw(entContinue);
	gameWindow->display();
}
