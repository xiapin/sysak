CC := gcc
CFLAG := -g -fpic
LDFLAG := -g -fpic -shared
OBJS := rbtree.o lrbtree.o
SO = lrbtree.so

all: install

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAG)

$(SO): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

install: $(SO)
	cp $(SO) ../lib

clean:
	rm -f $(SO) $(OBJS)

.PHONY : clean run