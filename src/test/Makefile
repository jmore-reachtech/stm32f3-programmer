CFLAGS = -c -Wall -g
INC = -I../../include -I../include
LIBS = ../../lib/libcommon.a

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))

all: ispd_client

ispd_client: $(OBJECTS)
	$(CC) -o ispd_client ispd_client.o 

%.o: %.c
	$(CC) $(CFLAGS) $(INC) $^ -o $@

clean:
	rm -f ispd_client $(OBJECTS)

.PHONY: clean
