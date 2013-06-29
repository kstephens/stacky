#ifndef _stacky_stacky_h
#define _stacky_stacky_h

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef void *voidP;
typedef char *charP;
typedef unsigned char *ucharP;

typedef void *stky_v;
typedef ssize_t stky_i;

typedef struct stacky_object {
  stky_v type;
  stky_i flags;
} stacky_object;

typedef struct stacky_type {
  stacky_object o;
  stky_i i;
  const char *name;
} stacky_type;

typedef struct stacky_array {
  stacky_object o;
  stky_v *b, *p;
  stky_i l, s, es;
} stacky_array;
typedef stacky_array *stacky_arrayP;

typedef struct stacky_string {
  stacky_object o;
  char *b, *p;
  stky_i l, s, es;
} stacky_string;
typedef stacky_string *stacky_stringP;

typedef struct stacky_symbol {
  stacky_object o;
  stacky_string *name;
} stacky_symbol;
typedef stacky_symbol *stacky_symbolP;

typedef struct stacky_dict {
  stacky_array a;
  stky_v eq;
} stacky_dict;
typedef stacky_dict *stacky_dictP;

#include "isn.h"

#define stky_v_bits 2
#define stky_v_mask 3
#define stky_v_tag(V)  (((stky_i) (V)) & 3)
#define stky_v_type(V)                                         \
  ( ! (V) ? Y->types + stky_t_null :                           \
    ! stky_v_tag(V) ? *(stky_v*)(V) :                          \
    Y->types + stky_v_tag(V) )
#define stky_v_int(X)   ((stky_v) ((((stky_i) (X)) << 2) | 1))
#define stky_v_int_(V)  (((stky_i) (V)) >> 2)
#define stky_v_char(X)  ((stky_v) ((((stky_i) (X)) << 2) | 2))
#define stky_v_char_(V) (((stky_i) (V)) >> 2)

enum stky_type_e {
#define TYPE(name,ind) stky_t_##name = ind,
#include "stacky/types.h"
  stky_t_END
};

typedef struct stacky {
  stacky_object o;
  stacky_array vs, es;
  stky_i trace, threaded_comp;
  stky_v v_stdin, v_stdout, v_stderr;
  stky_v v_mark;
  stacky_dict *sym_dict;
  stacky_array *dict_stack;
  stacky_type types[stky_t_END];
#define stky_t(name) (Y->types + stky_t_##name)
} stacky;
#define stky_v_mark (Y)->v_mark

stacky *stacky_new();
stacky *stacky_isn(stacky *Y, stky_i isn);
stacky *stacky_call(stacky *Y, stky_i *expr);
stky_v stacky_pop(stacky *Y);

stky_v stky_read_token(stacky *Y, FILE *in);

#endif
