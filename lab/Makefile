CFLAGS_OPT = -O3
CFLAGS += $(CFLAGS_OPT)
CFLAGS += -g
CFLAGS += -Wall

CFLAGS += -I../include/stacky
CFLAGS += -I/opt/local/include
LDFLAGS += -L/opt/local/lib
LDFLAGS += -lgc

all : stky2

stky2 : stky2.c

clean :
	rm -f stky2
