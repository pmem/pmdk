

TOP := $(dir $(lastword $(MAKEFILE_LIST)))../../../../
include $(TOP)/src/common.inc

SFML := $(call check_package, sfml-all)
ifeq ($(SFML),y)
PROGS = pmpong
else
$(info NOTE: Skipping pmpong because sfml is missing \
-- see src/examples/libpmemobj/pmpong/README for details.)
endif

LIBS = -lpmemobj -lpmem -lsfml-graphics -lsfml-window -lsfml-system

ifeq ($(SFML),y)
LIBS += $(shell $(PKG_CONFIG) --libs)
endif

COMPILE_LANG = cpp
include ../../Makefile.inc
INCS += -I../

pmpong: MainGame.o Paddle.o Ball.o PongGameStatus.o GameController.o Pool.o GameView.o MenuView.o GameOverView.o
