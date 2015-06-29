BIN = programmer

CFLAGS  = -g -Wall

INCLUDE = -I./include

all: main

main: main.c
	$(CC) $(CFLAGS) $(INCLUDE) main.c -o $(BIN)
	
deploy: main
	scp $(BIN) root@192.168.0.167:

clean:
	rm -f $(BIN)
	
PHONY: clean deploy
