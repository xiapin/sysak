CC := gcc
CFLAG := -g -fpic
LDFLAG := -g -fpic -shared
OBJS := postQue.o
SO := libpostQue.so

all: $(SO) install

%.o: %.c %.h
	$(CC) -c $< -o $@ $(CFLAG)

$(SO): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

install: $(SO)
	cp $(SO) ../lib

clean:
	rm -f *.so $(OBJS)
