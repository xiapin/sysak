LIB= -lpthread -ldl

CC=gcc
CFLAG := -g -I../beaver
LDFLAG := -g -lm -ldl -lrt -lpthread -lluajit-5.1 -L../beaver -lbeaver

PRG=bees
OBJ=apps.o bees.o beeQ.o
DEPMOD=../beaver ../collector/native ../tsdb/native

$(PRG): $(DEPMOD) $(OBJ)
	$(CC) $(LIB) -o $@ $(OBJ) $(LDFLAG)

%.o: %.c
	$(CC) -O $(CFLAG) -c $< -o $@

$(DEPMOD):ECHO
	make -C $@

ECHO:
	@echo $(SUBDIRS)

.PRONY:clean
clean:
	@echo "Removing linked and compiled files......"
	rm -f $(OBJ) $(PRG)
