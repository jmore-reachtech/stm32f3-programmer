CFLAGS = -c -Wall -g
INC = -I../include 

TARGET = libcommon.a

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(AR) rs $(TARGET) $(OBJECTS)

# You don't even need to be explicit here,
# compiling C files is handled automagically by Make.
%.o: %.c
	$(CC) $(CFLAGS) $(INC) $^ -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: clean
