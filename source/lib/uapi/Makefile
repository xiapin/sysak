SOURCE := $(shell find . -name "*.c")
OBJS :=$(patsubst %.c,%.o,$(SOURCE))
STATIC_OBJS := $(addprefix $(OBJPATH)/,$(OBJS))
LIBS += -L /usr/lib64 -l:libelf.a

libsysak: $(OBJ_LIB_PATH)/libsysak.a

$(OBJ_LIB_PATH)/libsysak.a: $(STATIC_OBJS)
	ar -rc $@ $^

$(STATIC_OBJS): $(OBJS)

$(OBJS): %.o : %.c
	gcc -c -o $(OBJPATH)/$@ $< -I$(SRC)/lib/uapi/include $(LIBS)


