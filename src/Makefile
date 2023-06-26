CFLAGS = -std=c89 -pedantic

all: master-build giocatori-build pedine-build

clean:
	rm -f *.o master giocatori pedine *~

master-build: master.c util.h util.c
	gcc $(CFLAGS) master.c util.c -o master

giocatori-build: giocatori.c util.h util.c
	gcc $(CFLAGS) giocatori.c util.c -o giocatori

pedine-build: pedine.c util.h util.c
	gcc $(CFLAGS) pedine.c util.c -o pedine

run: master
	./master