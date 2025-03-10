SHELL := /bin/sh

library: ksocket.o
	ar rs libksocket.a ksocket.o

ksocket.o: ksocket.h ksocket.c

init: library initksocket.c 
	gcc -g -I. -Wall -L. -o initk initksocket.c -lksocket

user: library user1.c user2.c
	gcc -g -I. -Wall -L. -o u1 user1.c -lksocket
	gcc -g -I. -Wall -L. -o u2 user2.c -lksocket

runinit: init 
	./initk

runuser: user
	@for i in 1; do \
		PORT_A=`expr 5000 + $$i \* 2 - 1`; \
		PORT_B=`expr 5000 + $$i \* 2`; \
		gnome-terminal -- bash -c "./u1 127.0.0.1 $$PORT_A 127.0.0.1 $$PORT_B; exec bash"; \
		gnome-terminal -- bash -c "./u2 127.0.0.1 $$PORT_B 127.0.0.1 $$PORT_A; exec bash"; \
	done

clean:
	rm -f ksocket.o initk u1 u2 received*.txt
	rm -f libksocket.a