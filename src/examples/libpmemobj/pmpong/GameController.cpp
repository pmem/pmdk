/*
 * Game.cpp
 *
 *  Created on: 12 lip 2017
 *      Author: huber
 */

#include "GameController.h"

#include "Pool.h"


Pool *gamePoolG;


GameController::GameController(){
	gameStatus = nvml::obj::make_persistent<PongGameStatus>();
}


GameController::~GameController() {
	nvml::obj::transaction::exec_tx(gamePoolG->getGamePool()->getPoolToTransaction(), [&]{
		nvml::obj::delete_persistent<PongGameStatus>(gameStatus);
	});
}

void GameController::gameLooping(bool isOnlySimulation){
	sf::RenderWindow *gameWindow = new sf::RenderWindow(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), GAME_NAME);
	gameWindow->setFramerateLimit(FRAMERATE_LIMIT);

	sf::Font font;
	if(!font.loadFromFile(FONT_PATH)){
		throw std::runtime_error("Cannot load font from file");
	}

	View *menuView = new MenuView(font);
	View *gameView = new GameView(font);
	View *gameOverView = new GameOverView(font);

	while (gameWindow->isOpen()){
		sf::Event event;
		while(gameWindow->pollEvent(event)){
			if(event.type == sf::Event::Closed)
				gameWindow->close();
		}
		gameWindow->clear();


		if(isOnlySimulation){
			if(gameStatus->getGameState() != game_state::SIMULATE){
				resetGameStatus();
				gameStatus->setIsGameToResume(false);
				gameStatus->setGameState(game_state::SIMULATE);
			}
			gameMatchSimulation(gameWindow, gameView, isOnlySimulation);
		}
		else{
			if(gameStatus->getGameState() == game_state::MATCH){
				gameMatch(gameWindow, gameView);
			}
			else if(gameStatus->getGameState() == game_state::MENU){
				menu(gameWindow, menuView);
			}
			else if(gameStatus->getGameState() == game_state::SIMULATE){
				gameMatchSimulation(gameWindow, gameView, isOnlySimulation);
			}
			else if(gameStatus->getGameState() == game_state::GAME_OVER){
				gameOver(gameWindow, gameOverView);
			}
		}
	}
	delete menuView;
	delete gameView;
	delete gameOverView;
	delete gameWindow;
}

void GameController::gameOver(sf::RenderWindow *gameWindow, View *view){

	view->prepareView(*gameStatus);
	view->displayView(gameWindow);

	sf::Event event;
	while(gameWindow->pollEvent(event)){
		if(event.type == sf::Event::KeyPressed){
			switch(event.key.code){
			case sf::Keyboard::Return:
				gameStatus->setIsGameToResume(false);
				gameStatus->setGameState(game_state::MENU);
				break;
			default:
				break;
			}
		}
		if(event.type == sf::Event::Closed)
			gameWindow->close();
	}

}

void GameController::menu(sf::RenderWindow *gameWindow, View *view){

	view->prepareView(*gameStatus);
	view->displayView(gameWindow);

	sf::Event event;
	while(gameWindow->pollEvent(event)){
		if(event.type == sf::Event::KeyPressed){
			switch(event.key.code){
			case sf::Keyboard::Up:
				gameStatus->setMenuItem(gameStatus->getMenuItem() == 0 ? MENU_ITEMS - 1 : gameStatus->getMenuItem() - 1);
				break;
			case sf::Keyboard::Down:
				gameStatus->setMenuItem((gameStatus->getMenuItem() + 1) % MENU_ITEMS);
				break;
			case sf::Keyboard::Return:
				if(gameStatus->getMenuItem() == NEW_GAME){
					resetGameStatus();
					gameStatus->setIsGameToResume(true);
					gameStatus->setGameState(game_state::MATCH);
				}
				else if(gameStatus->getMenuItem() == RESUME && gameStatus->getIsGameToResume()){
					gameStatus->setGameState(game_state::MATCH);
				}
				else if(gameStatus->getMenuItem() == SIMULATION){
					resetGameStatus();
					gameStatus->setIsGameToResume(false);
					gameStatus->setGameState(game_state::SIMULATE);
				}
				else if(gameStatus->getMenuItem() == EXIT){
					gameWindow->close();
				}
				break;
			default:
				break;
			}
		}
		if(event.type == sf::Event::Closed)
			gameWindow->close();
	}
}


void GameController::gameMatch(sf::RenderWindow *gameWindow, View *view){
	if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
		gameStatus->startBall(BALL_PLAYERS_SPEED);
	gameStatus->movePaddles();
	gameStatus->lookForCollisions(true);
	gameStatus->actualizeStatus();

	view->prepareView(*gameStatus);
	view->displayView(gameWindow);

	if(gameStatus->score()){
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
			gameStatus->startBall(BALL_PLAYERS_SPEED);
	}
	if(gameStatus->checkIfAnyPlayerWon()){
		gameStatus->setGameState(game_state::GAME_OVER);
	}
	else if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape)){
		gameStatus->setGameState(game_state::MENU);
	}
}

void GameController::gameMatchSimulation(sf::RenderWindow *gameWindow, View *view, bool isOnlySimulation){
	gameStatus->startBall(BALL_COMUTER_SPEED);
	gameStatus->simulate();
	gameStatus->lookForCollisions(false);
	gameStatus->actualizeStatus();
	if(gameStatus->score())
		gameStatus->startBall(BALL_COMUTER_SPEED);

	view->prepareView(*gameStatus);
	view->displayView(gameWindow);

	if(gameStatus->checkIfAnyPlayerWon()){
		gameStatus->setGameState(game_state::GAME_OVER);
	}
	else if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape)){
		gameStatus->setGameState(game_state::MENU);
	}
}

void GameController::resetGameStatus(){
	nvml::obj::transaction::exec_tx(gamePoolG->getGamePool()->getPoolToTransaction(), [&]{
		nvml::obj::delete_persistent<PongGameStatus>(gameStatus);
		gameStatus = nvml::obj::make_persistent<PongGameStatus>();
	});
}







