CC := gcc
AR := ar
CFLAG := -g
OBJS := outline.o
LIB := liboutline.a

all: $(LIB)


%.o: %.c, %.h
	$(CC) -c $< -o $@ $(CFLAG)

$(LIB): $(OBJS)
	$(AR) cr $@ $(OBJS)

clean:
	rm -f $(LIB) $(OBJS)