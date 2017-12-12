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

#include "GameOverView.hpp"

GameOverView::GameOverView(sf::Font &font)
{
	gameOver.setString("GAME OVER");
	entContinue.setString("press ENTER to continue");
	playerWinner.setString("");
	gameOver.setFont(font);
	playerWinner.setFont(font);
	entContinue.setFont(font);
	gameOver.setCharacterSize(GAMEOVER_FONTSIZE);
	playerWinner.setCharacterSize(MENUVIEW_ITEMS_FONTSIZE);
	entContinue.setCharacterSize(MENUVIEW_ITEMS_FONTSIZE);
	gameOver.setPosition(
		WINDOW_WIDTH / 2 - gameOver.getGlobalBounds().width / 2, 0);
	playerWinner.setPosition(
		WINDOW_WIDTH / 2 - playerWinner.getGlobalBounds().width / 2,
		GAMOVERVIEW_OFFSET * 2);
	entContinue.setPosition(WINDOW_WIDTH / 2 -
					entContinue.getGlobalBounds().width / 2,
				WINDOW_HEIGHT - GAMOVERVIEW_OFFSET);
	gameOver.setFillColor(sf::Color::Red);
	playerWinner.setFillColor(sf::Color::Green);
	entContinue.setFillColor(sf::Color::White);
}

GameOverView::~GameOverView()
{
}

void
GameOverView::prepareView(PongGameStatus &gameStatus)
{
	if (gameStatus.getPlayer1()->getPoints() == POINTS_TO_WIN)
		playerWinner.setString("LEFT PLAYER WON!");
	else
		playerWinner.setString("RIGHT PLAYER WON!");
}

void
GameOverView::displayView(sf::RenderWindow *gameWindow)
{
	gameWindow->clear();
	gameWindow->draw(gameOver);
	gameWindow->draw(playerWinner);
	gameWindow->draw(entContinue);
	gameWindow->display();
}
