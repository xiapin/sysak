LIB= -lpthread -ldl

CC=gcc
CFLAG := -g
LDFLAG := -g -lm -ldl -lpthread -lluajit-5.1

PRG=crash
OBJ=crash_test.o

$(PRG): $(OBJ)
	$(CC) $(LIB) -o $@ $(OBJ) $(LDFLAG)

%.o: %.c
	$(CC) -O $(CFLAG) -c $< -o $@

.PRONY:clean
clean:
	@echo "Removing linked and compiled files......"
	rm -f $(OBJ) $(PRG)
