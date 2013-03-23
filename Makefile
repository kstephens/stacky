CFLAGS += -g
CFLAGS += -O3

all: stacky inline

stacky : stacky.c

inline : inline.c

run-stacky: stacky
	gdb --args ./stacky

run-inline: inline
	gdb --args ./inline

clean:
	rm -f *.o stacky inline
