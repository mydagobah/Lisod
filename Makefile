################################################################################
# Makefile                                                                     #
#                                                                              #
# Description: This file contains the make rules for Recitation 1.             #
#                                                                              #
# Authors: Athula Balachandran <abalacha@cs.cmu.edu>,                          #
#          Wolf Richter <wolf@cs.cmu.edu>                                      #
#                                                                              #
################################################################################

default: lisod echo_client

lisod:
	@gcc lisod.c -o lisod -Wall -Werror

echo_client:
	@gcc echo_client.c -o echo_client -Wall -Werror

clean:
	@rm lisod echo_client
