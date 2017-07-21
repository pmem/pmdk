

TOP := $(dir $(lastword $(MAKEFILE_LIST)))../../../../
include $(TOP)/src/common.inc

SFML := $(call check_package, sfml)
ifeq ($(SFML),y)
PROGS = PmemONG
else
$(info NOTE: Skipping PmemONG because sfml is missing \
-- see src/examples/libpmemobj/PmemONG/README for details.)
endif

LIBS = -lpmemobj -lpmem

ifeq ($(SFML),y)
LIBS += $(shell $(PKG_CONFIG) --libs -lsfml-graphics -lsfml-window -lsfml-system)
endif

COMPILE_LANG = cpp
include ../../Makefile.inc
INCS += -I../

PmemONG: MainGame.o Paddle.o Ball.o PongGameStatus.o Game.o Pool.o
