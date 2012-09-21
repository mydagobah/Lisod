################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for Liso server project.      #
#                                                                              #
# Authors: Wenjun Zhang <wenjunzh@andrew.cmu.edu>                              #
#                                                                              #
################################################################################
CC = gcc
CFLAGS = -Wall -Werror -lefence

EXES = lisod 

all: $(EXES)

lisod:
	$(CC) $(CFLAGS) lisod.c log.c -o lisod

clean:
	@rm -rf $(EXES) lisod.log
