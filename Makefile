CC = clang
CFLAGS += -g
CFLAGS += -Iinclude
# CFLAGS += -O3

CFLAGS += -I/opt/local/include
LDFLAGS += -L/opt/local/lib
LDFLAGS += -lgc

LIB = src/libstacky.a
LIB_C := $(shell ls src/*.c)
LIB_O = $(LIB_C:.c=.o)

INCLUDE_H := $(shell ls include/*/*.h)

LDFLAGS += -Lsrc -lstacky

T_C = $(shell ls t/*.t.c)
T_T = $(T_C:%.c=%)

all: $(LIB) $(T_T)

$(LIB) : $(LIB_O)
	ar r $@ $(LIB_O)

$(LIB_O) : $(INCLUDE_H)

$(T_T) : $(LIB)
# $(T_T) :: $(INCLUDE_H)

%.s : %.c
	$(CC) $(CFLAGS) -S -o $@ $(@:.s=.c)

test: $(T_T)
	@for t in $(T_T); do \
	  (echo "+ $$t" ; $$t ; echo "exit($$?)") | tee $$t.out ;\
	done
	@error=0; for t in $(T_T); do \
	  diff -u $$t.exp $$t.out || error=1 ;\
	done ; exit $$error
	@echo test: OK

clean:
	rm -f src/*.o src/lib*.a t/*.t
	rm -rf t/*.dSYM

