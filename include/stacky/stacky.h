#ifndef _stacky_stacky_h
#define _stacky_stacky_h

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef ssize_t word_t;
typedef void *voidP;
typedef char *charP;
typedef unsigned char *ucharP;

typedef struct array {
  word_t *p;
  word_t l, s, es;
} array;
typedef array *arrayP;

typedef struct dict {
  word_t *p;
  word_t l, s, es;
  word_t eq;
} dict;
typedef dict *dictP;

#include "isn.h"

typedef struct stacky {
  word_t  *vb,  *vp; size_t vs;
  word_t **eb, **ep; size_t es;
  word_t trace, threaded_comp;
  word_t v_stdin, v_stdout, v_stderr;
  dict *ident_dict;
  array *dict_stack;
} stacky;

stacky *stacky_new();
stacky *stacky_isn(stacky *Y, word_t isn);
stacky *stacky_call(stacky *Y, word_t *expr);
word_t stacky_pop(stacky *Y);

#endif
