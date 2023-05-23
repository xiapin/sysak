CC := gcc
CFLAG := -g
LDFLAG := -g -lpcap
OBJS = filter.o
EXEC = filter

all: $(EXEC)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAG)

$(EXEC): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

clean:
	rm -f *.o $(EXEC)