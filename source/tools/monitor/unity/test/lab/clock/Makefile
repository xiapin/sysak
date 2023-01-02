CC=gcc
CFLAG := -g -O0
LDFLAG := -g -lrt

PRG=clock
OBJ=clock.o

$(PRG): $(OBJ)
	$(CC) $(LIB) -o $@ $(OBJ) $(LDFLAG)

%.o: %.c
	$(CC) -O $(CFLAG) -c $< -o $@

.PRONY:clean
clean:
	@echo "Removing linked and compiled files......"
	rm -f $(OBJ) $(PRG)