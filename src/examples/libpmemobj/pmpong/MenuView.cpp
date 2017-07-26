/*
 * MenuView.cpp
 *
 *  Created on: 25 lip 2017
 *      Author: huber
 */

#include "MenuView.h"

MenuView::MenuView(sf::Font &font) {
	menuItems[NEW_GAME].setString("NEW GAME");
	menuItems[RESUME].setString("RESUME");
	menuItems[SIMULATION].setString("SIMULATION");
	menuItems[EXIT].setString("EXIT");

	for(int i = 0; i < MENU_ITEMS; i++){
		menuItems[i].setFont(font);
		menuItems[i].setCharacterSize(MENUVIEW_ITEMS_FONTSIZE);
		menuItems[i].setPosition(WINDOW_WIDTH / 2 - menuItems[i].getGlobalBounds().width / 2, (i + 1) * MENUITEM_OFFSET - MENUVIEW_ITEMS_FONTSIZE);
	}
}

MenuView::~MenuView() {}


void MenuView::prepareView(PongGameStatus &gameStatus){
	for(int i = 0; i < MENU_ITEMS; i++){
		if(i == gameStatus.getMenuItem())
			menuItems[i].setFillColor(sf::Color::Green);
		else if(i == RESUME && !gameStatus.getIsGameToResume()){
			menuItems[RESUME].setFillColor(sf::Color::White);
		}
		else{
			menuItems[i].setFillColor(sf::Color::Red);
		}
	}
}

void MenuView::displayView(sf::RenderWindow *gameWindow){
	gameWindow->clear();
	for(int i = 0; i < MENU_ITEMS; i++){
		gameWindow->draw(menuItems[i]);
	}
	gameWindow->display();
}

