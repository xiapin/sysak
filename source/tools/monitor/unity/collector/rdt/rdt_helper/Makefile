CC := gcc
CFLAG := -g -fPIC
LDFLAG := -g -fpic -shared
OBJS := rdt_helper.o
SO := librdt_helper.so

# DEPMOD= ../collector
all: $(SO) install

%.o: %.c %.h
	$(CC) -c $< -o $@ $(CFLAG)

$(SO): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

install: $(SO)
	cp $(SO) ../../native/
	
clean:
	rm -f *.so $(OBJS)
