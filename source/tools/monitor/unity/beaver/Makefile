CC := gcc
AR := ar
CFLAG := -g
OBJS := beaver.o echos.o
LIB := libbeaver.a

all: $(LIB)

echos.o: echos.c beaver.h echos.h
	$(CC) -c $< -o $@ $(CFLAG)

beaver.o: beaver.c beaver.h echos.h
	$(CC) -c $< -o $@ $(CFLAG)

$(LIB): $(OBJS)
	$(AR) cr $@ $(OBJS)

clean:
	rm -f $(EXEC) $(OBJS)
