/*
 * View.h
 *
 *  Created on: 25 lip 2017
 *      Author: huber
 */

#ifndef VIEW_H_
#define VIEW_H_
#include <SFML/Graphics.hpp>
#include "GameConstants.h"
#include "PongGameStatus.h"

class View{
public:
	virtual ~View(){};
	virtual void prepareView(PongGameStatus &gameStatus) = 0;
	virtual void displayView(sf::RenderWindow *gameWindow) = 0;
};



#endif /* VIEW_H_ */
