all:	test sample run

LOPT=`uname | grep SunOS | sed 's/SunOS/-lnsl -lsocket/'`

test:	test.c Makefile
	gcc -Wall -g -o test test.c $(LOPT)

sample:	Server.c Makefile
	gcc -Wall -g -o Server Server.c $(LOPT)

run:
	./test ./Server
