appserver: appserver.c Bank.c
	gcc -lpthread -o appserver appserver.c Bank.c

appserver-coarse: appserver-coarse.c Bank.c
	gcc -lpthread -o appserver-coarse appserver-coarse.c Bank.c  
