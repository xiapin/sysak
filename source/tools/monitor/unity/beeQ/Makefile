LIB= -lpthread -ldl

CC=gcc
CFLAG := -g -I../beaver -I../collector/outline
LDFLAG := -g -lm -ldl -lrt -lpthread -lluajit-5.1 -L./lib/ -lbeeQ -L../beaver -lbeaver -lcollectorApi -L../collector/outline/ -loutline -L../collector/plugin/ -lproto_sender

PRG=unity-mon
OBJ=apps.o bees.o
DEPMOD=lib ../beaver ../collector/native ../collector/interface ../collector/outline ../collector/plugin ../tsdb/native

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
	$(foreach N, $(DEPMOD),make clean -C $(N);)
	rm -f $(OBJ) $(PRG)

dist:$(PRG)
	@echo "move dist/ to tools/"
	./pack.sh $(OBJPATH)/.sysak_components/tools/
