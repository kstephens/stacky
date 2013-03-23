CFLAGS += -g
# CFLAGS += -O3

all: inline

inline : inline.c

run-inline: inline
	gdb --args ./inline

clean:
	rm -f *.o stacky inline
