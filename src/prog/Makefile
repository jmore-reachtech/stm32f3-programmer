CFLAGS = -c -Wall -g
INC = -I../../include -I../include
LIBS = ../../lib/libcommon.a

TARGET = isp

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) $(LIBS) 

%.o: %.c
	$(CC) $(CFLAGS) $(INC) $^ -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: clean
