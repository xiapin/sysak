CC := gcc
CFLAG := -g
LDFLAG := -g
OBJS := thread_poll.o
EXEC := thread_poll

all: $(EXEC)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAG)

$(EXEC): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

clean:
	rm -f $(SO) $(OBJS)