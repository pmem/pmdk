/*
 * MenuView.h
 *
 *  Created on: 25 lip 2017
 *      Author: huber
 */

#ifndef MENUVIEW_H_
#define MENUVIEW_H_
#include <SFML/Graphics.hpp>
#include "View.h"
#include "GameConstants.h"
#include "PongGameStatus.h"

#define NEW_GAME 0
#define RESUME 1
#define SIMULATION 2
#define EXIT 3

class MenuView: public View {
public:
	MenuView(sf::Font &font);
	~MenuView();

	virtual void prepareView(PongGameStatus &gameStatus);
	virtual void displayView(sf::RenderWindow *gameWindow);


private:
	sf::Text menuItems[MENU_ITEMS];
};

#endif /* MENUVIEW_H_ */
