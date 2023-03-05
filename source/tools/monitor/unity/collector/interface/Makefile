CC := gcc
CFLAG := -g -fpic
LDFLAG := -g -fpic -shared
OBJS :=  unity_interface.o sig_stop.o fastKsym.o
SO := libcollectorApi.so

all: $(SO) install

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAG)

$(SO): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

install: $(SO)
	cp $(SO) ../../beeQ/lib

clean:
	rm -f $(SO) $(OBJS)