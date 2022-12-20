CC := gcc
CFLAG := -g
LDFLAG := -g -lm -ldl -lpthread -lluajit-5.1
OBJS := beaver.o main.o echos.o
EXEC := beaver

all: $(EXEC)

echos.o: echos.c beaver.h echos.h
	$(CC) -c $< -o $@ $(CFLAG)

beaver.o: beaver.c beaver.h echos.h
	$(CC) -c $< -o $@ $(CFLAG)

main.o: main.c beaver.h
	$(CC) -c $< -o $@ $(CFLAG)

$(EXEC): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

clean:
	rm -f $(EXEC) $(OBJS)
