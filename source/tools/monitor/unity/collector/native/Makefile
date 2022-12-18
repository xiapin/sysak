CC := gcc
CFLAG := -g -fpic
LDFLAG := -g -fpic -shared
OBJS := procffi.o
SO := libprocffi.so

all: $(SO)

procffi.o: procffi.c
	$(CC) -c $< -o $@ $(CFLAG)

$(SO): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

clean:
	rm -f $(SO) $(OBJS)