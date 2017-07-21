/*
 * Game.cpp
 *
 *  Created on: 12 lip 2017
 *      Author: huber
 */

#include "Game.h"
#include "Pool.h"


Pool *gamePoolG;


Game::Game(){
	board = nvml::obj::make_persistent<PongGameStatus>();
	actualGameState = game_state::MENU;
	menuItem = 0;
	isGameToResume = false;
}


Game::~Game() {
	nvml::obj::transaction::exec_tx(gamePoolG->getGamePool()->getPoolToTransaction(), [&]{
		nvml::obj::delete_persistent<PongGameStatus>(board);
	});
}

void Game::gameLooping(bool isOnlySimulation){
	sf::RenderWindow *gameWindow = new sf::RenderWindow(sf::VideoMode(WINDOW_WIDTH, WINDOW_HEIGHT), GAME_NAME);
	sf::Font font;

	if(!font.loadFromFile("NotoSansCJK-Regular.ttc")){
		throw std::runtime_error("Cannot load font from file");
	}
	gameWindow->setFramerateLimit(FRAMERATE_LIMIT);
	while (gameWindow->isOpen()){
		sf::Event event;
		while(gameWindow->pollEvent(event)){
			if(event.type == sf::Event::Closed)
				gameWindow->close();
		}
		gameWindow->clear();

		if(isOnlySimulation){
			if(actualGameState != game_state::COMPUTER_PLAY){
				setNewGame();
				setIsGameToResume(false);
				setGameState(game_state::COMPUTER_PLAY);
			}
			gameView(gameWindow, font, isOnlySimulation);
		}
		else{
			if(actualGameState == game_state::MATCH){
				gameView(gameWindow, font, false);
			}
			else if(actualGameState == game_state::MENU){
				menuView(gameWindow, font);
			}
			else if(actualGameState == game_state::COMPUTER_PLAY){
				gameView(gameWindow, font, true);
			}
			else if(actualGameState == game_state::GAME_OVER){
				gameOverView(gameWindow, font);
			}
		}
	}
	delete gameWindow;
}

void Game::gameOverView(sf::RenderWindow *gameWindow, sf::Font &font){
	sf::Text gameOver;
	sf::Text playerWinner;
	sf::Text entContinue;
	gameOver.setFont(font);
	playerWinner.setFont(font);
	entContinue.setFont(font);
	gameOver.setString("GAME OVER");

	if(board->getPlayer1()->getPoints() == POINTS_TO_WIN)
		playerWinner.setString("LEFT PLAYER WON!");
	else
		playerWinner.setString("RIGHT PLAYER WON!");

	entContinue.setString("press ENTER to continue");
	gameOver.setCharacterSize(GAMEOVER_FONTSIZE);
	playerWinner.setCharacterSize(MENUVIEW_ITEMS_FONTSIZE);
	entContinue.setCharacterSize(MENUVIEW_ITEMS_FONTSIZE);
	gameOver.setPosition(WINDOW_WIDTH / 2 - gameOver.getGlobalBounds().width / 2, 0);
	playerWinner.setPosition(WINDOW_WIDTH / 2 - playerWinner.getGlobalBounds().width / 2, GAMOVERVIEW_OFFSET * 2);
	entContinue.setPosition(WINDOW_WIDTH / 2 - entContinue.getGlobalBounds().width / 2, WINDOW_HEIGHT - GAMOVERVIEW_OFFSET);
	gameOver.setFillColor(sf::Color::Red);
	playerWinner.setFillColor(sf::Color::Green);
	entContinue.setFillColor(sf::Color::White);

	gameWindow->draw(gameOver);
	gameWindow->draw(playerWinner);
	gameWindow->draw(entContinue);

	gameWindow->display();

	sf::Event event;
	while(gameWindow->pollEvent(event)){
		if(event.type == sf::Event::KeyPressed){
			switch(event.key.code){
			case sf::Keyboard::Return:
				setIsGameToResume(false);
				setGameState(game_state::MENU);
				break;
			default:
				break;
			}
		}
		if(event.type == sf::Event::Closed)
			gameWindow->close();
	}

}

void Game::menuView(sf::RenderWindow *gameWindow, sf::Font &font){
	gameWindow->clear();

	sf::Text menuItems[MENU_ITEMS];
	menuItems[0].setString("NEW GAME");
	menuItems[1].setString("RESUME");
	menuItems[2].setString("SIMULATION");
	menuItems[3].setString("EXIT");

	for(int i = 0; i < MENU_ITEMS; i++){
		menuItems[i].setFont(font);
		menuItems[i].setCharacterSize(MENUVIEW_ITEMS_FONTSIZE);
		menuItems[i].setPosition(WINDOW_WIDTH / 2 - menuItems[i].getGlobalBounds().width / 2, (i + 1) * MENUITEM_OFFSET - MENUVIEW_ITEMS_FONTSIZE);
		if(i == menuItem)
			menuItems[i].setFillColor(sf::Color::Green);
		else if(i == 1 && !isGameToResume){
			menuItems[1].setFillColor(sf::Color::White);
		}
		else{
			menuItems[i].setFillColor(sf::Color::Red);
		}
		gameWindow->draw(menuItems[i]);
	}

	gameWindow->display();

	sf::Event event;
	while(gameWindow->pollEvent(event)){
		if(event.type == sf::Event::KeyPressed){
			switch(event.key.code){
			case sf::Keyboard::Up:
				setMenuItem(menuItem == 0 ? MENU_ITEMS - 1 : menuItem - 1);
				break;
			case sf::Keyboard::Down:
				setMenuItem((menuItem + 1) % MENU_ITEMS);
				break;
			case sf::Keyboard::Return:
				if(menuItem == 0){
					setNewGame();
					setIsGameToResume(true);
					setGameState(game_state::MATCH);
				}
				else if(menuItem == 1 && isGameToResume){
					setGameState(game_state::MATCH);
				}
				else if(menuItem == 2){
					setNewGame();
					setIsGameToResume(false);
					setGameState(game_state::COMPUTER_PLAY);
				}
				else if(menuItem == 3){
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

void Game::setGameState(game_state state){
	nvml::obj::transaction::exec_tx(gamePoolG->getGamePool()->getPoolToTransaction(), [&]{
		this->actualGameState = state;
	});
}

void Game::setMenuItem(int numb){
	nvml::obj::transaction::exec_tx(gamePoolG->getGamePool()->getPoolToTransaction(), [&]{
		this->menuItem = numb;
	});
}

void Game::gameView(sf::RenderWindow *gameWindow, sf::Font &font, bool isSimulation){
	sf::Text scoreP1;
	sf::Text scoreP2;

	sf::RectangleShape upperLine;
	sf::RectangleShape downLine;
	sf::RectangleShape leftLine;
	sf::RectangleShape rightLine;
	sf::RectangleShape court;


	scoreP1.setFont(font);
	scoreP2.setFont(font);
	scoreP1.setCharacterSize(GAMEVIEW_SCORE_FONTSIZE);
	scoreP2.setCharacterSize(GAMEVIEW_SCORE_FONTSIZE);
	scoreP1.setPosition(WINDOW_WIDTH/2 - SCORE_VIEW_OFFSET, SCORE_VIEW_OFFSET);
	scoreP2.setPosition(WINDOW_WIDTH/2 + SCORE_VIEW_OFFSET - scoreP2.getGlobalBounds().width, SCORE_VIEW_OFFSET);
	scoreP1.setFillColor(sf::Color::Green);
	scoreP2.setFillColor(sf::Color::Green);

	upperLine.setPosition(VERTICAL_LINE_OFFSET, scoreP1.getPosition().y + HORIZONAL_LINE_OFFSET);
	upperLine.setSize(sf::Vector2f(WINDOW_WIDTH - 2 * VERTICAL_LINE_OFFSET, LINE_THICKNESS));
	upperLine.setFillColor(sf::Color(224,224,224));

	downLine.setPosition(VERTICAL_LINE_OFFSET, WINDOW_HEIGHT - HORIZONAL_LINE_OFFSET);
	downLine.setSize(sf::Vector2f(WINDOW_WIDTH - 2 * VERTICAL_LINE_OFFSET + LINE_THICKNESS, LINE_THICKNESS));
	downLine.setFillColor(sf::Color(224,224,224));

	leftLine.setPosition(VERTICAL_LINE_OFFSET, scoreP1.getPosition().y + HORIZONAL_LINE_OFFSET);
	leftLine.setSize(sf::Vector2f(LINE_THICKNESS, WINDOW_HEIGHT - (scoreP1.getPosition().y + 2 * HORIZONAL_LINE_OFFSET)));
	leftLine.setFillColor(sf::Color(224,224,224));

	rightLine.setPosition(WINDOW_WIDTH - VERTICAL_LINE_OFFSET, scoreP1.getPosition().y + HORIZONAL_LINE_OFFSET);
	rightLine.setSize(sf::Vector2f(LINE_THICKNESS, WINDOW_HEIGHT - (scoreP1.getPosition().y + 2 * HORIZONAL_LINE_OFFSET)));
	rightLine.setFillColor(sf::Color(224,224,224));

	court.setPosition(VERTICAL_LINE_OFFSET + LINE_THICKNESS, scoreP1.getPosition().y + HORIZONAL_LINE_OFFSET);
	court.setSize(sf::Vector2f(WINDOW_WIDTH - 2 * VERTICAL_LINE_OFFSET, WINDOW_HEIGHT - (scoreP1.getPosition().y + 2 * HORIZONAL_LINE_OFFSET)));
	court.setFillColor(sf::Color(60,132,48));

	if(isSimulation){
		board->startBall(BALL_COMUTER_SPEED);
		board->simulation();
		board->lookForCollisions(false);
	}
	else{
		if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
			board->startBall(BALL_PLAYERS_SPEED);
		board->movePaddles();
		board->lookForCollisions(true);
	}
	board->actualizeStatus();
	if(board->score()){
		if(isSimulation)
			board->startBall(BALL_COMUTER_SPEED);
		else if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Space))
			board->startBall(BALL_PLAYERS_SPEED);
	}

	scoreP1.setString(std::to_string(board->getPlayer1()->getPoints()));
	scoreP2.setString(std::to_string(board->getPlayer2()->getPoints()));
	gameWindow->draw(court);
	gameWindow->draw(upperLine);
	gameWindow->draw(leftLine);
	gameWindow->draw(downLine);
	gameWindow->draw(rightLine);
	gameWindow->draw(scoreP1);
	gameWindow->draw(scoreP2);
	gameWindow->draw(board->getBall()->getBallShape());
	gameWindow->draw(board->getPlayer1()->getPaddleShape());
	gameWindow->draw(board->getPlayer2()->getPaddleShape());

	gameWindow->display();

	if(board->getPlayer1()->getPoints() == POINTS_TO_WIN || board->getPlayer2()->getPoints() == POINTS_TO_WIN){
		setGameState(game_state::GAME_OVER);
	}
	else if(sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Escape)){
		setGameState(game_state::MENU);
	}
}

void Game::setNewGame(){
	nvml::obj::transaction::exec_tx(gamePoolG->getGamePool()->getPoolToTransaction(), [&]{
		nvml::obj::delete_persistent<PongGameStatus>(board);
		board = nvml::obj::make_persistent<PongGameStatus>();
	});
}

void Game::setIsGameToResume(bool isGameToRes){
	nvml::obj::transaction::exec_tx(gamePoolG->getGamePool()->getPoolToTransaction(), [&]{
		isGameToResume = isGameToRes;
	});
}




