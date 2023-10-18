CC := g++
CFLAG := -g -fpic
LDFLAG := -g -fpic -shared -static-libstdc++
OBJS := memcg.o offset.o memread.o
SO := libpodmem.so
LIBS += -L ${OBJ_LIB_PATH}/lib -lcoolbpf -lelf -lz -lpthread
LDFLAG +=  -Wall  $(LIBS)

all: $(SO) install

%.o: %.cpp
	$(CC) -c $< -o $@ $(CFLAG)

$(SO): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAG)

install: $(SO)
	cp $(SO) ../../native/

clean:
	rm -f $(SO) $(OBJS)
