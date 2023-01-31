CC := gcc
CFLAG := -g
LDFLAG := -g
OBJS := safe_popen.o main.o
EXEC := safe_popen

all: $(EXEC)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAG)

$(EXEC): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

clean:
	rm -f $(SO) $(OBJS)