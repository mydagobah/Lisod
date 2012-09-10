################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for Liso server project.      #
#                                                                              #
# Authors: Wenjun Zhang <wenjunzh@andrew.cmu.edu>                              #
#                                                                              #
################################################################################
CC = gcc
CFLAGS = -Wall -Werror #-lefence

EXES = lisod echo_client

default: $(EXES)

lisod:
	$(CC) $(CFLAGS) lisod.c log.c -o lisod

echo_client:
	$(CC) $(CFLAGS) echo_client.c -o echo_client

clean:
	@rm -rf $(EXES) lisod.log
