ISN(nul,1)
ISN(hdr,1)
ISN(hdr_,1)
ISN(nop,1)
ISN(rtn,1)
ISN(lint,2)
ISN(lstr,2)
ISN(dup,1)
ISN(pop,1)
ISN(swap,1)
ISN(ifelse,3)
ISN(writeint,1)
ISN(writestr,1)

#define BOP(name,op) ISN(name,1)
#define UOP(name,op) ISN(name,1)
#include "cops.h"
#undef ISN
