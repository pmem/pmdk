/*
 * GameView.h
 *
 *  Created on: 25 lip 2017
 *      Author: huber
 */

#ifndef GAMEVIEW_H_
#define GAMEVIEW_H_
#include <SFML/Graphics.hpp>
#include "View.h"
#include "GameConstants.h"
#include "PongGameStatus.h"
#include <string>

class GameView: public View {
public:
	GameView(sf::Font &font);
	~GameView();

	virtual void prepareView(PongGameStatus &gameStatus);
	virtual void displayView(sf::RenderWindow *gameWindow);

private:
	sf::Text scoreP1;
	sf::Text scoreP2;

	sf::RectangleShape upperLine;
	sf::RectangleShape downLine;
	sf::RectangleShape leftLine;
	sf::RectangleShape rightLine;
	sf::RectangleShape court;

	sf::CircleShape ballShape;
	sf::RectangleShape leftPaddleShape;
	sf::RectangleShape rightPaddleShape;
};

#endif /* GAMEVIEW_H_ */
