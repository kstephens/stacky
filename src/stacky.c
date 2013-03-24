#include "stacky/stacky.h"

struct isn_def {
  word_t isn;
  int nwords;
  const char *name;
  const char *isn_ident;
  void *addr;
};

static struct isn_def isn_table[] = {
#define ISN(name,nwords) { isn_##name, nwords, #name, "%" #name },
#include "stacky/isns.h"
  { 0, 0, 0 },
};

stacky *stacky_isn(stacky *Y, word_t isn)
{
  word_t expr[] = { isn_hdr, isn, isn_rtn, isn_END };
  return stacky_call(Y, expr);
}

array *stacky_array_init(stacky *Y, array *a, size_t es, size_t s)
{
  a->p = a->b = malloc((a->es = es) * (a->s = s));
  memset(a->b, 0, a->s * a->es);
  a->l = 0;
  return a;
}

stacky *stacky_array_resize(stacky *Y, array *a, size_t s)
{
  word_t *nb;
  // fprintf(stderr, "  array %p b:s %p:%lu p:l %p:%lu\n", a, a->b, (unsigned long) a->s, a->p, (unsigned long) a->l);  
  nb = realloc(a->b, a->es * s);
  a->s = s;
  if ( a->l > a->s ) a->l = a->s;
  a->p = nb + (a->p - a->b);
  a->b = nb;
  // fprintf(stderr, "  array %p b:s %p:%lu p:l %p:%lu\n\n", a, a->b, (unsigned long) a->s, a->p, (unsigned long) a->l);  
  return Y;
}

stacky *stacky_call(stacky *Y, word_t *pc)
{
  word_t  *vp = Y->vs.p,  *ve = Y->vs.b + Y->vs.s;
  word_t **ep = (void*) Y->es.p, **eb = (void*) Y->es.b;

#define PUSH(X) do {                                                  \
    word_t __tmp = (word_t) (X);                                      \
    if ( ++ vp >= ve ) {                                              \
      Y->vs.p = vp;                                                   \
      stacky_array_resize(Y, &Y->vs, Y->vs.s + 1024);                 \
      vp = Y->vs.p; ve = Y->vs.b + Y->vs.s;                           \
    }                                                                 \
    *vp = __tmp;                                                      \
  } while(0)
#define PUSHt(X,T) PUSH(X)
#define POP() -- vp
#define POPN(N) vp -= (N)
#define V(i) vp[- (i)]
#define Vt(i,t) (*((t*) (vp - (i))))
#define CALLISN(I) do {                           \
    -- Y->trace;                                  \
    Y->vs.p = vp;                                 \
    stacky_isn(Y, (I));                           \
    vp = Y->vs.p; ve = Y->vs.b + Y->vs.s;         \
    ++ Y->trace;                                  \
  } while (0)
#define CALL(E) do {                            \
    Y->vp = vp;                                 \
    stacky_call(Y, (E));                        \
    vp = Y->vp; ve = Y->vs.b + Y->vs.s;         \
  } while (0)

  if ( ! isn_table[0].addr ) {
    struct isn_def *isn;
#define ISN(name,lits) isn = &isn_table[isn_##name]; isn->addr = &&L_##name;
#include "stacky/isns.h"
  }

  if ( Y->threaded_comp && *pc == isn_hdr ) {
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
  if ( Y->trace > 0 ) {
    { 
      word_t **p = eb;
      fprintf(stderr, "  # E: ");
      while ( p < ep )
        fprintf(stderr, "@%p ", (void*) *(p ++));
      fprintf(stderr, "| \n");
    }
    {
      word_t *p = Y->vs.b;
      fprintf(stderr, "  # V: ");
      while ( p <= vp )
        fprintf(stderr, "%lld ", (long long) *(p ++));
      fprintf(stderr, "| \n");
    }
    fprintf(stderr, "  # P: @%p\n", (void*) pc);
    if ( *pc <= isn_END ) {
      fprintf(stderr, "  # I: %s ", isn_table[*pc].name);
      switch ( *pc ) {
      case isn_lint:
        fprintf(stderr, "%lld ", (long long) pc[1]); break;
      case isn_lcharP: case isn_ident_charP:
        fprintf(stderr, "\"%s\" ", (char*) pc[1]); break;
      case isn_ifelse:
        fprintf(stderr, "@%p @%p ", (void*) pc[1], (void*) pc[2]); break;
      case isn_ifelser:
        fprintf(stderr, "@%p @%p ", (void*) pc + 2 + pc[1], (void*) pc + 2 + pc[2]); break;
      }
    } else {
      fprintf(stderr, "  # I: %p ", *((void**) pc));
    }
    fprintf(stderr, "\n");
  }

#define ISN(name) goto next_isn; case isn_##name: L_##name
  switch ( (int) *(pc ++) ) {
  ISN(nul):   abort();
  ISN(hdr):
  ISN(hdr_):
  ISN(nop):
  ISN(lint):   PUSHt(*(pc ++),int);
  ISN(lcharP): PUSHt(*(pc ++),charP);
  ISN(lvoidP): PUSHt(*(pc ++),voidP);
  ISN(eq_charP): V(1) = ! strcmp(Vt(1, charP), Vt(0, charP)); POP();
  ISN(vget):  V(0) = V(V(0));
  ISN(vset):  V(V(1)) = V(0); POPN(2);
  ISN(dup):   PUSH(V(0));
  ISN(pop):   POP();
  ISN(swap):  { word_t tmp = V(0); V(0) = V(1); V(1) = tmp; }
#define BOP(name, op) ISN(name): V(1) = V(1) op V(0); POP();
#define UOP(name, op) ISN(name): V(0) = op V(0);
#include "stacky/cops.h"
  ISN(ifelser):
    pc += 2;
    pc[-3] = isn_ifelse;
    pc[-2] = (word_t) (pc + pc[-2]);
    pc[-1] = (word_t) (pc + pc[-1]);
    pc -= 2;
    goto L_ifelse;
  ISN(ifelse):
    if ( V(0) ) {
      POP(); pc = ((word_t**) pc)[0];
    } else {
      POP(); pc = ((word_t**) pc)[1];
    }
  ISN(jmpr):
    ++ pc;
    pc[-2] = isn_jmp;
    pc[-1] = (word_t) (pc + pc[-1]);
    pc = (word_t*) pc[-1];
  ISN(jmp):
    pc = (word_t*) pc[0];
  ISN(v_stdin):  PUSH(Y->v_stdin);
  ISN(v_stdout): PUSH(Y->v_stdout);
  ISN(v_stderr): PUSH(Y->v_stderr);
  ISN(write_int):
    fprintf(Vt(0,FILE*), "%lld", (long long) V(1)); POPN(2);
  ISN(write_charP):
    fprintf(Vt(0,FILE*), "%s", Vt(1,charP)); POPN(2);
  ISN(write_voidP):
    fprintf(Vt(0,FILE*), "@%p", Vt(1,voidP)); POPN(2);
  ISN(c_malloc):  Vt(0,voidP) = malloc(V(0));
  ISN(c_realloc): Vt(1,voidP) = realloc(Vt(1,voidP), V(0)); POP();
  ISN(c_free):    free(Vt(0,voidP)); POP();
  ISN(c_memmove): memmove(Vt(2,voidP), Vt(1,voidP), Vt(0,size_t)); POPN(3);
  ISN(array_new): {
      Vt(0,arrayP) = stacky_array_init(Y, malloc(sizeof(array)), sizeof(word_t), Vt(0,size_t));
    }
  ISN(array_ptr):  Vt(0,voidP)  = Vt(0,arrayP)->b;
  ISN(array_len):  Vt(0,size_t) = Vt(0,arrayP)->l;
  ISN(array_size): Vt(0,size_t) = Vt(0,arrayP)->s;
  ISN(array_get):  V(1) = Vt(1,arrayP)->b[V(0)]; POP();
  ISN(array_set):  Vt(2,arrayP)->b[V(1)] = V(0); POPN(2);
  ISN(array_push): {
      array *a = Vt(1,arrayP);
      if ( a->l >= a->s )
        stacky_array_resize(Y, a, a->l + 1);
      *(a->p = a->b + a->l ++) = V(0);
      POP();
    }
  ISN(array_pop): {
      array *a = Vt(0,array*);
      V(0) = *(a->p = a->b + -- a->l);
    }
  ISN(dict_new): {
      dict *d = malloc(sizeof(*d));
      stacky_array_init(Y, (array*) d, sizeof(word_t), 8);
      d->eq = V(0);
      // fprintf(stderr, "  dict_new %lld\n", (long long) d);
      Vt(0,dictP) = d;
    }
  ISN(dict_get): {
      dict *d = Vt(2,dictP);
      word_t k = V(1), v = V(0);
      size_t i = 0;
      while ( i < d->a.l ) {
        PUSH(k); PUSH(d->a.b[i]); PUSH(d->eq); CALLISN(isn_call);
        if ( V(0) ) {
          POP();
          V(2) = d->a.b[i + 1];
          goto dict_get_done;
        }
        POP();
        i += 2;
      }
      V(2) = v;
    dict_get_done:
      POPN(2);
    }
  ISN(dict_set): {
      dict *d = Vt(2,dictP);
      word_t k = V(1), v = V(0);
      size_t i = 0;
      while ( i < d->a.l ) {
        PUSH(k); PUSH(d->a.b[i]); PUSH(d->eq); CALLISN(isn_call);
        if ( V(0) ) {
          POP();
          d->a.b[i + 1] = V(0);
          POPN(2);
          goto dict_set_done;
        }
        POP();
        i += 2;
      }
      POPN(2);
      PUSH(k); CALLISN(isn_array_push);
      PUSH(v); CALLISN(isn_array_push);
    dict_set_done:
      (void) 0;
    }
  ISN(ident): {
      char *str = Vt(0,charP);
      PUSH(Y->ident_dict); PUSH(str); PUSH(0); CALLISN(isn_dict_get);
      if ( V(0) ) {
        V(1) = V(0); POP();
      } else {
        POP();
        str = strcpy(malloc(strlen(str) + 1), str);
        PUSH(Y->ident_dict); PUSH(str); PUSH(str); CALLISN(isn_dict_set);
        POP();
        // fprintf(stderr, "  ident %lld %s\n", (long long) str, str);
        Vt(0,charP) = str;
      }
    }
  ISN(ident_charP): {
      char *str = (void*) *(pc ++);
      PUSH(str); CALLISN(isn_ident);
      pc[-2] = isn_lcharP;
      pc[-1] = V(0);
    }
  ISN(dict_stack): PUSH(Y->dict_stack);
  ISN(dict_stack_top): {
      array *a = (void*) Y->dict_stack;
      PUSH(a->p[a->l - 1]);
    }
  ISN(lookup): {
      word_t k = V(0);
      int i = Y->dict_stack->l;
      while ( -- i >= 0 ) {
        PUSH(Y->dict_stack->p[i]); PUSH(k); PUSH((word_t) &k); CALLISN(isn_dict_get);
        if ( V(0) != (word_t) &k ) {
          V(1) = V(0); POP();
          goto lookup_done;
        }
        POP();
      }
      V(0) = 0; // NOT_FOUND
    lookup_done:
      (void) 0;
    }
  ISN(wget): V(1) = Vt(1,word_t*)[V(0)]; POP();
  ISN(wset): Vt(2,word_t*)[V(1)] = V(0); POPN(2);
  ISN(cget): V(1) = Vt(1,ucharP)[V(0)]; POP();
  ISN(cset): Vt(2,ucharP)[V(1)] = V(0); POPN(2);
  ISN(call):
    *(ep ++) = pc;
    pc = Vt(0,word_t*); POP();
  ISN(rtn):
    if ( ep <= eb ) goto rtn;
    pc = *(-- ep);
  ISN(Y): PUSH((word_t) Y);
  ISN(c_proc):
    switch ( *(pc ++) ) {
    case 0:
      ((void (*)()) V(0)) (); POPN(1); break;
    case 1:
      ((void (*)(word_t)) V(0)) (V(1)); POPN(2); break;
    case 2:
      ((void (*)(word_t, word_t)) V(0)) (V(2), V(1)); POPN(3); break;
    default: abort();
    }
  ISN(c_func):
    switch ( *(pc ++) ) {
    case 0:
      V(0) = ((word_t (*)()) V(0)) (); break;
    case 1:
      V(1) = ((word_t (*)(word_t)) V(0)) (V(1)); POPN(1); break;
    case 2:
      V(2) = ((word_t (*)(word_t, word_t)) V(0)) (V(2), V(1)); POPN(2); break;
    default: abort();
    }
  ISN(END): goto rtn;
  }
#undef ISN

 rtn:
  Y->vs.p = vp;
  Y->es.p = (void*) eb;
  return Y;
}

word_t stacky_pop(stacky *Y)
{
  return *(Y->vs.p --);
}

stacky *stacky_new()
{
  stacky *Y = malloc(sizeof(*Y));
  Y->v_stdin = (word_t) stdin;
  Y->v_stdout = (word_t) stdout;
  Y->v_stderr = (word_t) stderr;

  stacky_array_init(Y, &Y->vs, sizeof(word_t), 1024);
  *(Y->vs.p) = 0;
  stacky_array_init(Y, &Y->es, sizeof(void**), 1024);

  {
    static word_t e_eq_charP[] = { isn_eq_charP, isn_rtn, isn_END };
    static word_t e[] = { isn_lvoidP, (word_t) e_eq_charP, isn_dict_new, isn_rtn, isn_END }; 
    Y->ident_dict = (void*) stacky_pop(stacky_call(Y, e));
  }
  { 
    static word_t e[] = { isn_lint, 0, isn_array_new, isn_rtn, isn_END };
    Y->dict_stack = (void*) stacky_pop(stacky_call(Y, e));
  }
  {
    static word_t e_eq[] = { isn_hdr, isn_eq, isn_rtn, isn_END };
    word_t e[] = {
      isn_dict_stack,
      isn_lvoidP, (word_t) e_eq, isn_dict_new,
      isn_array_push, isn_pop,
      isn_rtn, isn_END };
    stacky_call(Y, e);
  }
  {
    int i;
    for ( i = 0; isn_table[i].name; ++ i ) {
      char name[32] = { 0 };
      word_t e_isn_[] = { isn_hdr, isn_table[i].isn, isn_rtn, isn_END };
      word_t *e_isn = memcpy(malloc(sizeof(e_isn_)), e_isn_, sizeof(e_isn_));
      word_t e[] = {
        isn_dict_stack_top,
        isn_ident_charP, (word_t) isn_table[i].isn_ident,
        isn_lvoidP, (word_t) e_isn,
        isn_dict_set, isn_pop,
        isn_rtn, isn_END };
      // ++ Y->trace;
      stacky_call(Y, e);
      // -- Y->trace;
    }
  }

  return Y;
}

