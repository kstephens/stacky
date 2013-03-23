ISN(nul,1)
ISN(hdr,1)
ISN(hdr_,1)
ISN(nop,1)
ISN(call,1)
ISN(rtn,1)
ISN(lint,2)
ISN(lcharP,2)
ISN(eq_charP,1)
ISN(lvoidP,2)
ISN(vget,1)
ISN(vset,1)
ISN(dup,1)
ISN(pop,1)
ISN(swap,1)
ISN(ifelse,3)
ISN(ifelser, 3)
ISN(jmp, 1)
ISN(jmpr, 2)
ISN(write_int,1)
ISN(write_charP,1)
ISN(write_voidP,1)
ISN(c_malloc,1)
ISN(c_realloc,1)
ISN(c_free,1)
ISN(c_memmove,1)
ISN(array_new,1)
ISN(array_ptr,1)
ISN(array_len,1)
ISN(array_size,1)
ISN(array_get,1)
ISN(array_set,1)
ISN(array_push,1)
ISN(array_pop,1)
ISN(dict_new,1)
ISN(dict_get,1)
ISN(dict_set,1)
ISN(ident,1)
ISN(ident_charP,2)
ISN(wget,1)
ISN(wset,1)
ISN(cget,1)
ISN(cset,1)

#define BOP(name,op) ISN(name,1)
#define UOP(name,op) ISN(name,1)
#include "cops.h"
#undef ISN
