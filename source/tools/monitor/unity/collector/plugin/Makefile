CC := gcc
CFLAG := -g -fpic
LDFLAG := -g -fpic -shared
OBJS := proto_sender.o
LIB := libproto_sender.a

DEPMOD=sample threads

all: $(LIB) $(DEPMOD)

%.o: %.c
	$(CC) -c $< -o $@ $(CFLAG)

$(LIB): $(OBJS)
	$(AR) cr $@ $(OBJS)

$(DEPMOD):ECHO
	make -C $@

ECHO:
	@echo $(DEPMOD)
