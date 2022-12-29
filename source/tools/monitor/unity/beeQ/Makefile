LIB= -lpthread -ldl

CC=gcc
CFLAG := -g -I../beaver
LDFLAG := -g -lm -ldl -lpthread -lluajit-5.1 -L../beaver -lbeaver

PRG=bees
OBJ=apps.o bees.o beeQ.o

$(PRG): $(OBJ)
	$(CC) $(LIB) -o $@ $(OBJ) $(LDFLAG)

%.o: %.c
	$(CC) -O $(CFLAG) -c $< -o $@

.PRONY:clean
clean:
	@echo "Removing linked and compiled files......"
	rm -f $(OBJ) $(PRG)
