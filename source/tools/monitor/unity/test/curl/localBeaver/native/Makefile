CC := gcc
CFLAG := -g -fpic
LDFLAG := -g -fpic -shared
OBJS := local_beaver.o
SO := liblbeaver.so

all: $(SO)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAG)

$(SO): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

clean:
	rm -f $(SO) $(OBJS)