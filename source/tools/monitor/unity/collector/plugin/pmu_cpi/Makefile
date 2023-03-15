CC := gcc
CFLAG := -g -fpic
LDFLAG := -g -fpic -shared
OBJS := pmu_cpi.o
SO := libpmu_cpi.so

all: $(SO) install

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAG)

$(SO): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

install: $(SO)
	cp $(SO) ../../native/

clean:
	rm -f $(SO) $(OBJS)
