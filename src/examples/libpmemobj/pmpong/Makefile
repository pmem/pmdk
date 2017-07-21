

TOP := $(dir $(lastword $(MAKEFILE_LIST)))../../../../
include $(TOP)/src/common.inc

SFML := $(call check_package, sfml-all --atleast-version 2.4)
ifeq ($(SFML),y)
PROGS = pmpong
else
$(info NOTE: Skipping pmpong because sfml is missing \
-- see src/examples/libpmemobj/pmpong/README for details.)
endif

LIBS = -lpmemobj -lpmem

ifeq ($(SFML),y)
LIBS += $(shell $(PKG_CONFIG) --libs sfml-all)
endif

COMPILE_LANG = cpp
include ../../Makefile.inc
INCS += -I../

pmpong: MainGame.o Paddle.o Ball.o PongGameStatus.o GameController.o Pool.o GameView.o MenuView.o GameOverView.o
