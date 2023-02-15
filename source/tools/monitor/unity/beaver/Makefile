CC := gcc
AR := ar
CFLAG := -g
OBJS := beaver.o
LIB := libbeaver.a
DEPMOD = native

all: $(DEPMOD) $(LIB)


beaver.o: beaver.c beaver.h
	$(CC) -c $< -o $@ $(CFLAG)

$(LIB): $(OBJS)
	$(AR) cr $@ $(OBJS)

$(DEPMOD):ECHO
	make -C $@

ECHO:
	@echo $(SUBDIRS)

clean:
	rm -f $(EXEC) $(OBJS)
