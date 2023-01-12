CC := gcc
CFLAG := -g -fpic
LDFLAG := -g -fpic -shared
OBJS := sample_threads.o
SO := libsample_threads.so

all: $(SO) install

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAG)

$(SO): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

install: $(SO)
	cp $(SO) ../../native/

clean:
	rm -f $(SO) $(OBJS)