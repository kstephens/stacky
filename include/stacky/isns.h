ISN(nul,1)
ISN(hdr,1)
ISN(hdr_,1)
ISN(nop,1)
ISN(call,1)
ISN(rtn,1)
ISN(lit,2)
ISN(lit_int,2)
ISN(lit_charP,2)
ISN(lit_voidP,2)
ISN(string_eq,1)
ISN(vget,1)
ISN(vset,1)
ISN(dup,1)
ISN(pop,1)
ISN(swap,1)
ISN(ifelse,3)
ISN(ifelser, 3)
ISN(jmp, 1)
ISN(jmpr, 2)
ISN(v_stdin,1)
ISN(v_stdout,1)
ISN(v_stderr,1)
ISN(write_int,1)
ISN(write_char,1)
ISN(write_symbol,1)
ISN(write_string,1)
ISN(write_voidP,1)
ISN(c_malloc,1)
ISN(c_realloc,1)
ISN(c_free,1)
ISN(c_memmove,1)
ISN(v_tag,1)
ISN(v_type,1)
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
ISN(dict_stack,1)
ISN(dict_stack_top,1)
ISN(lookup,1)
ISN(ident,1)
ISN(ident_charP,2)
ISN(Y,1)
ISN(c_proc,2)
ISN(c_func,2)

#define BOP(name,op) ISN(name,1)
#define UOP(name,op) ISN(name,1)
#include "cops.h"
#undef ISN
