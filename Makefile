#CFLAGS_OPT = -O2
CFLAGS_OPT = 
CFLAGS += $(CFLAGS_OPT)
CFLAGS += -g
CFLAGS += -Wall
CFLAGS+= -momit-leaf-frame-pointer

CFLAGS += -Igen
CFLAGS += -Iboot
CFLAGS += -I/opt/local/include
LDFLAGS += -L/opt/local/lib
LDFLAGS += -lgc

all : early stky

early : gen/prims.h gen/types.h

stky : stky.c

gen/prims.h : *.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Ds_F=s_F -E $< | perl -ne 'while ( s~(s_F\([^\)]+\))~~ ) { print "def_", $$1, "\n"; }' | sort -u > $@
	echo "#undef def_s_F" >> $@

gen/types.h : *.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Ds_F=s_F -E $< | perl -ne 'while ( s~struct s_(\w+)~~ ) { print "TYPE(", $$1, ")\n"; }' | sort -u > $@
	echo "#undef TYPE" >> $@

clean :
	rm -f stky gen/* stky.s

stky.s : early

%.s : %.c
	$(CC) $(CFLAGS) -S -o - $(@:.s=.c) | tool/asm-source > $@
