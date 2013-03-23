CFLAGS += -g
CFLAGS += -Iinclude
# CFLAGS += -O3

LIB = src/libstacky.a
LIB_C = $(shell ls src/*.c)
LIB_O = $(LIB_C:.c=.o)

LDFLAGS += -Lsrc -lstacky

T_C = $(shell ls t/*.t.c)
T_T = $(T_C:%.c=%)

all: $(LIB) $(T_T)

$(LIB) : $(LIB_O)
	ar r $@ $(LIB_O)

$(T_T) : $(LIB)

clean:
	rm -f src/*.o src/lib*.a t/*.t t/*.dSYM

