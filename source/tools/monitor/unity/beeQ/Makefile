LIB= -lpthread -ldl

CC=gcc
CFLAG := -g -I../beaver
LDFLAG := -g -lm -ldl -lrt -lpthread -lluajit-5.1 -L../beaver -lbeaver -L../collector/plugin/ -lproto_sender

PRG=unity-mon
OBJ=apps.o bees.o beeQ.o
DEPMOD=../beaver ../collector/native ../collector/plugin ../tsdb/native

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
