/*
 * GameOverView.h
 *
 *  Created on: 25 lip 2017
 *      Author: huber
 */

#ifndef GAMEOVERVIEW_H_
#define GAMEOVERVIEW_H_
#include <SFML/Graphics.hpp>
#include "View.h"
#include "GameConstants.h"
#include "PongGameStatus.h"

class GameOverView: public View {
public:
	GameOverView(sf::Font &font);
	~GameOverView();

	virtual void prepareView(PongGameStatus &gameStatus);
	virtual void displayView(sf::RenderWindow *gameWindow);

private:
	sf::Text gameOver;
	sf::Text playerWinner;
	sf::Text entContinue;
};

#endif /* GAMEOVERVIEW_H_ */
