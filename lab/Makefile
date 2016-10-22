CFLAGS_OPT = -O3
CFLAGS += $(CFLAGS_OPT)
CFLAGS += -g
CFLAGS += -Wall

CFLAGS += -Igen
CFLAGS += -Iboot
CFLAGS += -I/opt/local/include
LDFLAGS += -L/opt/local/lib
LDFLAGS += -lgc

all : gen/prims.h gen/types.h stky2

stky2 : stky2.c

gen/prims.h : *.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Dstky_F=stky_F -E $< | perl -ne 'while ( s~(stky_F\([^\)]+\))~~ ) { print "def_", $$1, "\n"; }' | sort -u > $@

gen/types.h : *.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -Dstky_F=stky_F -E $< | perl -ne 'while ( s~struct stky_(\w+)~~ ) { print "TYPE(", $$1, ")\n"; }' | sort -u > $@
	echo "#undef TYPE" >> $@

clean :
	rm -f stky2 gen/*

%.s : %.c
	$(CC) $(CFLAGS) -S -o - $(@:.s=.c) | ../tool/asm-source > $@
