
CFLAGS  = -g -Wall

INCLUDE = -I./include

all: main

main: main.c
	$(CC) $(CFLAGS) $(INCLUDE) main.c -o serial
	
deploy: main
	scp serial root@192.168.0.167:

clean:
	rm -f serial
	
PHONY: clean deploy
