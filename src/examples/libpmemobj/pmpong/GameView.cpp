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

#include "GameView.hpp"

GameView::GameView(sf::Font &font)
{
	sf::Color elementsColor(224, 224, 224);
	scoreP1.setFont(font);
	scoreP2.setFont(font);
	scoreP1.setCharacterSize(GAMEVIEW_SCORE_FONTSIZE);
	scoreP2.setCharacterSize(GAMEVIEW_SCORE_FONTSIZE);

	scoreP1.setPosition(WINDOW_WIDTH / 2 - SCORE_VIEW_OFFSET,
			    SCORE_VIEW_OFFSET);
	scoreP2.setPosition(WINDOW_WIDTH / 2 + SCORE_VIEW_OFFSET -
				    scoreP2.getGlobalBounds().width,
			    SCORE_VIEW_OFFSET);
	scoreP1.setFillColor(sf::Color::Green);
	scoreP2.setFillColor(sf::Color::Green);

	upperLine.setPosition(VERTICAL_LINE_OFFSET,
			      scoreP1.getPosition().y + HORIZONAL_LINE_OFFSET);
	upperLine.setSize(sf::Vector2f(WINDOW_WIDTH - 2 * VERTICAL_LINE_OFFSET,
				       LINE_THICKNESS));
	upperLine.setFillColor(elementsColor);

	downLine.setPosition(VERTICAL_LINE_OFFSET,
			     WINDOW_HEIGHT - HORIZONAL_LINE_OFFSET);
	downLine.setSize(sf::Vector2f(WINDOW_WIDTH - 2 * VERTICAL_LINE_OFFSET +
					      LINE_THICKNESS,
				      LINE_THICKNESS));
	downLine.setFillColor(elementsColor);

	leftLine.setPosition(VERTICAL_LINE_OFFSET,
			     scoreP1.getPosition().y + HORIZONAL_LINE_OFFSET);
	leftLine.setSize(sf::Vector2f(
		LINE_THICKNESS, WINDOW_HEIGHT -
			(scoreP1.getPosition().y + 2 * HORIZONAL_LINE_OFFSET)));
	leftLine.setFillColor(elementsColor);

	rightLine.setPosition(WINDOW_WIDTH - VERTICAL_LINE_OFFSET,
			      scoreP1.getPosition().y + HORIZONAL_LINE_OFFSET);
	rightLine.setSize(sf::Vector2f(
		LINE_THICKNESS, WINDOW_HEIGHT -
			(scoreP1.getPosition().y + 2 * HORIZONAL_LINE_OFFSET)));
	rightLine.setFillColor(elementsColor);

	court.setPosition(VERTICAL_LINE_OFFSET + LINE_THICKNESS,
			  scoreP1.getPosition().y + HORIZONAL_LINE_OFFSET);
	court.setSize(sf::Vector2f(
		WINDOW_WIDTH - 2 * VERTICAL_LINE_OFFSET, WINDOW_HEIGHT -
			(scoreP1.getPosition().y + 2 * HORIZONAL_LINE_OFFSET)));
	court.setFillColor(sf::Color(60, 132, 48));

	ballShape.setRadius(BALL_SIZE);
	ballShape.setPosition(sf::Vector2f(0, 0));
	ballShape.setFillColor(elementsColor);

	rightPaddleShape.setSize(sf::Vector2f(PADDLE_WIDTH, PADDLE_HEIGHT));
	leftPaddleShape.setSize(sf::Vector2f(PADDLE_WIDTH, PADDLE_HEIGHT));
	leftPaddleShape.setPosition(sf::Vector2f(0, 0));
	rightPaddleShape.setPosition(sf::Vector2f(0, 0));
	leftPaddleShape.setFillColor(sf::Color::Red);
	rightPaddleShape.setFillColor(sf::Color::Red);
}

GameView::~GameView()
{
}

void
GameView::prepareView(PongGameStatus &gameStatus)
{
	scoreP1.setString(std::to_string(gameStatus.getPlayer1()->getPoints()));
	scoreP2.setString(std::to_string(gameStatus.getPlayer2()->getPoints()));

	ballShape.setPosition(
		sf::Vector2f((float)gameStatus.getBall()->getX(),
			     (float)gameStatus.getBall()->getY()));
	leftPaddleShape.setPosition(
		sf::Vector2f((float)gameStatus.getPlayer1()->getX(),
			     (float)gameStatus.getPlayer1()->getY()));
	rightPaddleShape.setPosition(
		sf::Vector2f((float)gameStatus.getPlayer2()->getX(),
			     (float)gameStatus.getPlayer2()->getY()));
}

void
GameView::displayView(sf::RenderWindow *gameWindow)
{
	gameWindow->clear();

	gameWindow->draw(court);
	gameWindow->draw(upperLine);
	gameWindow->draw(leftLine);
	gameWindow->draw(downLine);
	gameWindow->draw(rightLine);
	gameWindow->draw(scoreP1);
	gameWindow->draw(scoreP2);
	gameWindow->draw(ballShape);
	gameWindow->draw(leftPaddleShape);
	gameWindow->draw(rightPaddleShape);

	gameWindow->display();
}
