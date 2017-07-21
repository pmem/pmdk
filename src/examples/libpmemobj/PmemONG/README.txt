Non-Volatile Memory Library

This is examples/libpmemobj/PmemONG/README.

This directory contains an example application implemented using libpmemobj,
it's a game in which all the objects are stored on persistent memory.
This means that the game process can be safely killed and then resumed.

To launch the game:
	./PmemONG <game session file> [mode]

Mode option might be skipped if u want to run game with GUI or use
	-s
to run game simulation.

The file with the game session will either be created if it doesn't exist
or opened if it contains a valid pool.

Controls:
	move left paddle - up and down arrow keys
	move right paddle - 'w' and 's'
	pause - esc
	start ball - space

This game demonstrates the usage of the very basics of the libpmemobj C++
bindings. It demonstrates pool management, persistent pointers and transactions.

** DEPENDENCIES: **
In order to build the game you need to install SFML library.

