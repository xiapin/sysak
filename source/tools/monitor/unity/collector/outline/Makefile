CC := gcc
AR := ar
CFLAG := -g
OBJS := outline.o
LIB := liboutline.a

all: $(DEPMOD) $(LIB)


%.o: %.c, %.h
	$(CC) -c $< -o $@ $(CFLAG)

$(LIB): $(OBJS)
	$(AR) cr $@ $(OBJS)

clean:
	rm -f $(EXEC) $(OBJS)