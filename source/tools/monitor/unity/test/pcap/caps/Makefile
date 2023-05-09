CC := gcc
CFLAG := -g
LDFLAG := -g -lpcap
OBJS = caps.o
EXEC = caps

all: $(EXEC)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAG)

$(EXEC): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

clean:
	rm -f $(SO) $(EXEC)