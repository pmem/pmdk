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

#ifndef GAMEVIEW_H_
#define GAMEVIEW_H_
#include "GameConstants.hpp"
#include "PongGameStatus.hpp"
#include "View.hpp"
#include <SFML/Graphics.hpp>
#include <string>

class GameView : public View {
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
