CC := gcc
CFLAG := -g
LDFLAG := -g -lpcap
OBJS = capLoop.o
EXEC = capLoop

all: $(EXEC)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAG)

$(EXEC): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

clean:
	rm -f *.o $(EXEC)