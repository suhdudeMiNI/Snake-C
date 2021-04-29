all : tsnake
CC = gcc
CFLAGS = -std=gnu99 -Wall
LDLIBS = -lpthread -lm

tsnake : tsnake.c
	${CC} -o tsnake tsnake.c ${CFLAGS} ${LDLIBS} 

_PHONY : clean all

clean: 
	rm tsnake