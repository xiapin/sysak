CC := gcc
CFLAG := -g -fpic
LDFLAG := -g -fpic -shared
OBJS := uncore_imc.o
SO := libuncore_imc.so

all: $(SO) install

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAG)

$(SO): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

install: $(SO)
	cp $(SO) ../../native/

clean:
	rm -f $(SO) $(OBJS)