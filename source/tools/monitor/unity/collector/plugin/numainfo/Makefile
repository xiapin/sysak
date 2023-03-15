CC := gcc
CFLAG := -g -fpic -lnuma
LDFLAG := -g -fpic -shared -lnuma
OBJS := numainfo.o
SO := libnumainfo.so

all: $(SO) install

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAG)

$(SO): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

install: $(SO)
	cp $(SO) ../../native/

clean:
	rm -f $(SO) $(OBJS)
