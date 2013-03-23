#include <stdlib.h>
#include <stdio.h>
#include <string.h>

typedef ssize_t word_t;
typedef struct stacky {
  word_t v_stdin, v_stdout, v_stderr;
  word_t word_dict;
} stacky;

struct isn_def {
  word_t isn;
  int nwords;
  const char *name;
  void *addr;
};

#include "isn.h"

static struct isn_def isn_table[] = {
#define ISN(name,nwords) { isn_##name, nwords, #name, },
#include "isns.h"
  { 0, 0, 0 },
};

static word_t threaded_comp;
static word_t trace;

void stacky_go(stacky *Y, word_t *vp, size_t vs, word_t *pc)
{
  word_t *vb = malloc(sizeof(vb[0]) * (vs + 1024));
  memcpy(vb, vp, sizeof(vb[0]) * vs);
  vp = vb + vs - 1;
  vs += 1024;

#define PUSH(X) do {                                      \
    word_t __tmp = (word_t) (X);                          \
    if ( ++ vp > vb + vs ) {                              \
      vs += 1024;                                         \
      word_t *nvb = realloc(vb, sizeof(vb[0] * vs));      \
      vp = (vp - vb) + nvb;                               \
      vb = nvb;                                           \
    }                                                     \
    *vp = __tmp;                                          \
  } while(0)
#define POP() -- vp
#define V(i) vp[- (i)]

  if ( ! isn_table[0].addr ) {
    struct isn_def *isn;
#define ISN(name,lits) isn = &isn_table[isn_##name]; isn->addr = &&label_##name;
#include "isns.h"
  }

  if ( threaded_comp && *pc == isn_hdr ) {
    word_t *pc_save = pc;
    *(pc ++) = isn_hdr_;
    while ( *pc != isn_END ) {
      struct isn_def *isn = &isn_table[*pc];
      fprintf(stderr, "  %4d %10s => @%p\n", (int) (pc - pc_save), isn->name, isn->addr);
      *pc = (word_t) isn->addr;
      pc += isn->nwords;
    }
    *pc = (word_t) &abort;
    pc = pc_save;
    pc ++;
    goto *((void**) (pc ++));
  }

 next_isn:
  if ( trace ) {
    word_t *p = vb;
    while ( p <= vp )
      fprintf(stderr, "%lld ", (long long) *(p ++));
    fprintf(stderr, "| @%p ", (void*) pc);
    if ( *pc <= isn_END ) {
      fprintf(stderr, "%s ", isn_table[*pc].name);
      switch ( *pc ) {
      case isn_lint:
        fprintf(stderr, "%lld ", (long long) pc[1]); break;
      case isn_lstr:
        fprintf(stderr, "\"%s\" ", (char*) pc[1]); break;
      case isn_ifelse:
        fprintf(stderr, "@%p @%p ", (void*) pc[1], (void*) pc[2]); break;
      }
    } else {
      fprintf(stderr, "%p ", *((void**) pc));
    }
    fprintf(stderr, "\n");
  }

#define ISN(name) goto next_isn; case isn_##name: label_##name
  switch ( (int) *(pc ++) ) {
  ISN(nul):   abort();
  ISN(nop):   ;
  ISN(rtn):   goto rtn;
  ISN(lint):  PUSH(*(pc ++));
  ISN(lstr):  PUSH(*(pc ++));
  ISN(dup):   PUSH(V(0));
  ISN(pop):   POP();
  ISN(swap):  { word_t tmp = V(0); V(0) = V(1); V(1) = tmp; }
#define BOP(name, op) ISN(name): V(1) = V(1) op V(0); POP();
#define UOP(name, op) ISN(name): V(0) = op V(0);
#include "cops.h"
  ISN(ifelse):
    if ( V(0) ) {
      POP(); pc = ((word_t**) pc)[0];
    } else {
      POP(); pc = ((word_t**) pc)[1];
    }
  ISN(writeint):
    fprintf(stdout, "%lld", (long long) V(0)); POP();
  ISN(writestr):
    fprintf(stdout, "%s", (char*) V(0)); POP();
  ISN(hdr):
  ISN(hdr_):
  ISN(END):   goto rtn;
  }

 rtn:
  free(vb);
}

int main(int argc, char **argv)
{
  word_t t[] = {
    isn_hdr,
    isn_lstr, (word_t) " true\n",
    isn_writestr,
    isn_END,
  };
  word_t f[] = {
    isn_hdr,
    isn_lstr, (word_t) " false\n",
    isn_writestr,
    isn_END,
  };
  word_t isns[] = {
    isn_hdr,
    isn_lint, 2,
    isn_lint, 3,
    isn_add,
    isn_lint, 5,
    isn_mul,
    isn_dup,
    isn_writeint,
    isn_lint, 25,
    isn_ge,
    isn_ifelse, (word_t) t, (word_t) f,
    isn_rtn,
    isn_END,
  };
  stacky *Y = malloc(sizeof(*Y));
  stacky_go(Y, 0, 0, isns);
  return 0;
}
