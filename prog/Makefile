CFLAGS = -c -Wall -g
INC = -I../include 
LIBS = ../common/libcommon.a

TARGET = isp

OBJECTS = $(patsubst %.c, %.o, $(wildcard *.c))

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -o $(TARGET) $(OBJECTS) $(LIBS) 

# You don't even need to be explicit here,
# compiling C files is handled automagically by Make.
%.o: %.c
	$(CC) $(CFLAGS) $(INC) $^ -o $@

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: clean
