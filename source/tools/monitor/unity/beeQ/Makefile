LIB= -lpthread -ldl

CC=gcc
CFLAG := -g -I../beaver -I../collector/outline
LDFLAG := -g -lm -ldl -lrt -lpthread -lluajit-5.1 -L./lib/ -lbeeQ -L../beaver -lbeaver -lcollectorApi -Lclock/ -llocalclock -LpostQue -lpostQue -L../collector/outline/ -loutline -L../collector/plugin/ -lproto_sender

PRG=unity-mon
OBJ=apps.o bees.o daemon.o pushTo.o
DEPMOD=lib rbtree clock postQue ../beaver ../collector/native ../collector/interface ../collector/outline ../collector/plugin ../tsdb/native ../collector/container/cg_pmu_events_ffi ../common/protobuf/metricstore

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
