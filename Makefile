BIN = programmer

CFLAGS  = -g -Wall

INCLUDE = -I./include

SRCS = main.c serial.c gpio.c stm32.c

all: main

main: $(SRCS)
	$(CC) $(CFLAGS) $(INCLUDE) $(SRCS) -o $(BIN)
	
deploy: main
	scp $(BIN) root@192.168.0.167:

clean:
	rm -f $(BIN)
	
PHONY: clean deploy
