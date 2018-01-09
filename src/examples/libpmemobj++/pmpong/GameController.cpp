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

#include "GameController.hpp"
#include "Pool.hpp"

Pool *gamePoolG;

GameController::GameController()
{
	gameStatus = pmem::obj::make_persistent<PongGameStatus>();
}

GameController::~GameController()
{
	pmem::obj::transaction::exec_tx(
		gamePoolG->getGamePool()->getPoolToTransaction(), [&] {
			pmem::obj::delete_persistent<PongGameStatus>(
				gameStatus);
		});
}

void
GameController::gameLoop(bool isSimulation)
{
	sf::RenderWindow *gameWindow = new sf::RenderWindow(
		sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), GAME_NAME);
	gameWindow->setFramerateLimit(FRAMERATE_LIMIT);

	sf::Font font;
	if (!font.loadFromFile(FONT_PATH)) {
		throw std::runtime_error("Cannot load font from file");
	}

	View *menuView = new MenuView(font);
	View *gameView = new GameView(font);
	View *gameOverView = new GameOverView(font);

	while (gameWindow->isOpen()) {
		sf::Event event;
		while (gameWindow->pollEvent(event)) {
			if (event.type == sf::Event::Closed)
				gameWindow->close();
		}
		gameWindow->clear();
		if (isSimulation) {
			if (gameStatus->getGameState() !=
			    game_state::SIMULATE) {
				resetGameStatus();
				gameStatus->setIsGameToResume(false);
				gameStatus->setGameState(game_state::SIMULATE);
			}
			gameMatchSimulation(gameWindow, gameView, isSimulation);
		} else {
			if (gameStatus->getGameState() == game_state::MATCH) {
				gameMatch(gameWindow, gameView);
			} else if (gameStatus->getGameState() ==
				   game_state::MENU) {
				menu(gameWindow, menuView);
			} else if (gameStatus->getGameState() ==
				   game_state::SIMULATE) {
				gameMatchSimulation(gameWindow, gameView,
						    isSimulation);
			} else if (gameStatus->getGameState() ==
				   game_state::GAME_OVER) {
				gameOver(gameWindow, gameOverView);
			}
		}
	}
	delete menuView;
	delete gameView;
	delete gameOverView;
	delete gameWindow;
}

void
GameController::gameOver(sf::RenderWindow *gameWindow, View *view)
{
	view->prepareView(*gameStatus);
	view->displayView(gameWindow);

	sf::Event event;
	while (gameWindow->pollEvent(event)) {
		if (event.type == sf::Event::KeyPressed) {
			switch (event.key.code) {
				case sf::Keyboard::Return:
					gameStatus->setIsGameToResume(false);
					gameStatus->setGameState(
						game_state::MENU);
					break;
				default:
					break;
			}
		}
		if (event.type == sf::Event::Closed)
			gameWindow->close();
	}
}

void
GameController::menu(sf::RenderWindow *gameWindow, View *view)
{
	view->prepareView(*gameStatus);
	view->displayView(gameWindow);

	sf::Event event;
	while (gameWindow->pollEvent(event)) {
		if (event.type == sf::Event::KeyPressed) {
			handleEventKeypress(event, gameWindow);
		}
		if (event.type == sf::Event::Closed)
			gameWindow->close();
	}
}

void
GameController::handleEventKeypress(sf::Event &event,
				    sf::RenderWindow *gameWindow)
{
	switch (event.key.code) {
		case sf::Keyboard::Up:
			gameStatus->setMenuItem(
				gameStatus->getMenuItem() == 0
					? MENU_ITEMS - 1
					: gameStatus->getMenuItem() - 1);
			break;
		case sf::Keyboard::Down:
			gameStatus->setMenuItem(
				(gameStatus->getMenuItem() + 1) % MENU_ITEMS);
			break;
		case sf::Keyboard::Return:
			if (gameStatus->getMenuItem() == NEW_GAME) {
				resetGameStatus();
				gameStatus->setIsGameToResume(true);
				gameStatus->setGameState(game_state::MATCH);
			} else if (gameStatus->getMenuItem() == RESUME &&
				   gameStatus->getIsGameToResume()) {
				gameStatus->setGameState(game_state::MATCH);
			} else if (gameStatus->getMenuItem() == SIMULATION) {
				resetGameStatus();
				gameStatus->setIsGameToResume(false);
				gameStatus->setGameState(game_state::SIMULATE);
			} else if (gameStatus->getMenuItem() == EXIT) {
				gameWindow->close();
			}
			break;
		default:
			break;
	}
}

void
GameController::gameMatch(sf::RenderWindow *gameWindow, View *view)
{
	if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
		gameStatus->startBall(BALL_PLAYERS_SPEED);
	gameStatus->movePaddles();
	gameStatus->lookForCollisions(true);
	gameStatus->actualizeStatus();

	view->prepareView(*gameStatus);
	view->displayView(gameWindow);

	if (gameStatus->score()) {
		if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
			gameStatus->startBall(BALL_PLAYERS_SPEED);
	}
	if (gameStatus->checkIfAnyPlayerWon()) {
		gameStatus->setGameState(game_state::GAME_OVER);
	} else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape)) {
		gameStatus->setGameState(game_state::MENU);
	}
}

void
GameController::gameMatchSimulation(sf::RenderWindow *gameWindow, View *view,
				    bool isSimulation)
{
	gameStatus->startBall(BALL_COMPUTER_SPEED);
	gameStatus->simulate();
	gameStatus->lookForCollisions(false);
	gameStatus->actualizeStatus();
	if (gameStatus->score())
		gameStatus->startBall(BALL_COMPUTER_SPEED);

	view->prepareView(*gameStatus);
	view->displayView(gameWindow);

	if (gameStatus->checkIfAnyPlayerWon()) {
		gameStatus->setGameState(game_state::GAME_OVER);
	} else if (sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape)) {
		gameStatus->setGameState(game_state::MENU);
	}
}

void
GameController::resetGameStatus()
{
	pmem::obj::transaction::exec_tx(
		gamePoolG->getGamePool()->getPoolToTransaction(), [&] {
			pmem::obj::delete_persistent<PongGameStatus>(
				gameStatus);
			gameStatus =
				pmem::obj::make_persistent<PongGameStatus>();
		});
}
