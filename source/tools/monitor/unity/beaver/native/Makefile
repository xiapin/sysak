CC := gcc
CFLAG := -g -fpic
LDFLAG := -g -fpic -shared -lssl -lcrypto
OBJS := local_beaver.o async_ssl.o
SO := liblbeaver.so

all: $(SO) install

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAG)

$(SO): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

install: $(SO)
	cp $(SO) ../../beeQ/lib

clean:
	rm -f $(SO) $(OBJS)