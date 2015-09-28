CFLAGS = -c -Wall -g
INC = -I../include 

TARGET = libcommon.a

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rs $(TARGET) $(OBJECTS)

%.o: %.c
	$(CC) $(CFLAGS) $(INC) $^ -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: clean
