SRC=npb.c

TARGET=npb.so
CFLAGS=-std=gnu99

all:$(TARGET)

$(TARGET):$(SRC)
	gcc -O3 -shared -fPIC $(SRC) $(CFLAGS) -o $@
