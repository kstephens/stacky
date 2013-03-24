#include "stacky/stacky.h"

struct isn_def {
  stky_i isn;
  int nwords;
  const char *name;
  const char *isn_sym;
  void *addr;
};

static struct isn_def isn_table[] = {
#define ISN(name,nwords) { isn_##name, nwords, #name, "%" #name },
#include "stacky/isns.h"
  { 0, 0, 0 },
};

stacky *stacky_isn(stacky *Y, stky_i isn)
{
  stky_i expr[] = { isn_hdr, isn, isn_rtn, isn_END };
  return stacky_call(Y, expr);
}

stacky_array *stacky_array_init(stacky *Y, stacky_array *a, size_t es, size_t s)
{
  size_t cs;
  a->o.type = stky_t(array);
  a->p = a->b = malloc(cs = (a->es = es) * ((a->s = s) + 1));
  memset(a->b, 0, cs);
  a->l = 0;
  return a;
}

stacky_array *stacky_array_dup(stacky *Y, stacky_array *a)
{
  stacky_array *b = stacky_array_init(Y, malloc(sizeof(*a)), a->es, a->s);
  b->o.type = a->o.type;
  memcpy(b->b, a->b, a->es * a->l);
  return a;
}

stacky_string *stky_string_new_charP(stacky *Y, char *p, size_t s)
{
  stacky_string *o;
  if ( ! p ) return 0;
  if ( s == -1LL ) s = strlen(p);
  o = malloc(sizeof(*o));
  stacky_array_init(Y, (void*) o, 1, s + 1);
  o->o.type = stky_t(string);
  memcpy(o->b, p, s + 1);
  o->l = s;
  return o;
}

stacky *stacky_array_resize(stacky *Y, stacky_array *a, size_t s)
{
  stky_v *nb;
  // fprintf(stderr, "  array %p b:s %p:%lu p:l %p:%lu\n", a, a->b, (unsigned long) a->s, a->p, (unsigned long) a->l);  
  nb = realloc(a->b, a->es * s);
  a->s = s;
  if ( a->l > a->s ) a->l = a->s;
  a->p = nb + (a->p - a->b);
  a->b = nb;
  // fprintf(stderr, "  array %p b:s %p:%lu p:l %p:%lu\n\n", a, a->b, (unsigned long) a->s, a->p, (unsigned long) a->l);  
  return Y;
}

stacky *stacky_call(stacky *Y, stky_i *pc)
{
  stky_v  *vp = Y->vs.p,  *ve = Y->vs.b + Y->vs.s;
  stky_i **ep = (void*) Y->es.p, **eb = (void*) Y->es.b;

#define PUSH(X) do {                                                  \
    stky_v __tmp = (stky_v) (X);                                      \
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
#define Vi(i) stky_v_int_(V(i))
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
    stky_i *pc_save = pc;
    *(pc ++) = isn_hdr_;
    while ( *pc != isn_END ) {
      struct isn_def *isn = &isn_table[*pc];
      fprintf(stderr, "  %4d %10s => @%p\n", (int) (pc - pc_save), isn->name, isn->addr);
      *pc = (stky_i) isn->addr;
      pc += isn->nwords;
    }
    *pc = (stky_i) &abort;
    pc = pc_save;
    pc ++;
    goto *((void**) (pc ++));
  }

 next_isn:
  if ( Y->trace > 0 ) {
    { 
      stky_i **p = eb;
      fprintf(stderr, "  # E: ");
      while ( p < ep )
        fprintf(stderr, "@%p ", (void*) *(p ++));
      fprintf(stderr, "| \n");
    }
    {
      stky_v *p = Y->vs.b;
      fprintf(stderr, "  # V: ");
      while ( p <= vp )
        fprintf(stderr, "%p ", (void*) *(p ++));
      fprintf(stderr, "| \n");
    }
    fprintf(stderr, "  # P: @%p\n", (void*) pc);
    if ( *pc <= isn_END ) {
      fprintf(stderr, "  # I: %s ", isn_table[*pc].name);
      switch ( *pc ) {
      case isn_lit: case isn_lit_voidP:
        fprintf(stderr, "%p ", (void*) pc[1]); break;
      case isn_lit_int:
        fprintf(stderr, "%lld ", (long long) pc[1]); break;
      case isn_lit_charP: case isn_sym_charP:
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
  ISN(lit):       PUSH((stky_v) *(pc ++));
  ISN(lit_int):   PUSH(stky_v_int(*(pc ++)));
  ISN(lit_charP): { stky_v v = stky_string_new_charP(Y, (void*) *(pc ++), -1LL);
      pc[-2] = isn_lit; pc[-1] = (stky_i) v; PUSH(v); }
  ISN(lit_voidP): PUSHt(*(pc ++),voidP);
  ISN(string_eq): {
      stacky_string *a = V(1), *b = V(0);
      V(1) = stky_v_int(a == b || (a->l == b->l && ! memcmp(a->b, b->b, a->l))); 
      POP(); }
  ISN(vget):  V(0) = V(Vi(0));
  ISN(vset):  V(Vi(1)) = V(0); POPN(2);
  ISN(dup):   PUSH(V(0));
  ISN(pop):   POP();
  ISN(swap):  { stky_v tmp = V(0); V(0) = V(1); V(1) = tmp; }
#define BOP(name, op) ISN(name): V(1) = stky_v_int(Vi(1) op Vi(0)); POP();
#define UOP(name, op) ISN(name): V(0) = stky_v_int(op Vi(0));
#include "stacky/cops.h"
  ISN(ifelser):
    pc += 2;
    pc[-3] = isn_ifelse;
    pc[-2] = (stky_i) (pc + pc[-2]);
    pc[-1] = (stky_i) (pc + pc[-1]);
    pc -= 2;
    goto L_ifelse;
  ISN(ifelse):
    if ( Vi(0) ) {
      POP(); pc = ((stky_i**) pc)[0];
    } else {
      POP(); pc = ((stky_i**) pc)[1];
    }
  ISN(jmpr):
    ++ pc;
    pc[-2] = isn_jmp;
    pc[-1] = (stky_i) (pc + pc[-1]);
    pc = (stky_i*) pc[-1];
  ISN(jmp):
    pc = (stky_i*) pc[0];
  ISN(v_stdin):  PUSH(Y->v_stdin);
  ISN(v_stdout): PUSH(Y->v_stdout);
  ISN(v_stderr): PUSH(Y->v_stderr);
  ISN(write_int):
    fprintf(Vt(0,FILE*), "%lld", (long long) Vi(1)); POPN(2);
  ISN(write_char):
    fprintf(Vt(0,FILE*), "%c", (int) stky_v_char_(V(1))); POPN(2);
  ISN(write_symbol): {
      stacky_symbol *s = Vt(1,stacky_symbolP); 
      V(1) = s->name; goto L_write_string; }
  ISN(write_string): { 
      stacky_string *s = Vt(1,stacky_stringP); 
      fwrite(s->b, 1, s->l, Vt(0,FILE*)); POPN(2); }
  ISN(write_voidP):
    fprintf(Vt(0,FILE*), "@%p", Vt(1,voidP)); POPN(2);
  ISN(c_malloc):  Vt(0,voidP) = malloc(Vi(0));
  ISN(c_realloc): Vt(1,voidP) = realloc(Vt(1,voidP), Vi(0)); POP();
  ISN(c_free):    free(Vt(0,voidP)); POP();
  ISN(c_memmove): memmove(Vt(2,voidP), Vt(1,voidP), Vi(0)); POPN(3);
  ISN(v_tag):     V(0) = stky_v_int(stky_v_tag(V(0)));
  ISN(v_type):    V(0) = stky_v_type(V(0));
  ISN(array_new):
      Vt(0,stacky_arrayP) =
        stacky_array_init(Y, stky_malloc(sizeof(stacky_array)), sizeof(stky_v), Vi(0));
  ISN(array_ptr):  V(0) = Vt(0,stacky_arrayP)->b;
  ISN(array_len):  V(0) = stky_v_int(Vt(0,stacky_arrayP)->l);
  ISN(array_size): V(0) = stky_v_int(Vt(0,stacky_arrayP)->s);
  ISN(array_get):  V(1) = Vt(1,stacky_arrayP)->b[Vi(0)]; POP();
  ISN(array_set):  Vt(2,stacky_arrayP)->b[Vi(1)] = V(0); POPN(2);
  ISN(array_push): {
      stacky_array *a = Vt(1,stacky_arrayP);
      if ( a->l >= a->s )
        stacky_array_resize(Y, a, a->l + 1);
      *(a->p = a->b + a->l ++) = V(0);
      POP(); }
  ISN(array_pop): {
      stacky_array *a = Vt(0,stacky_array*);
      V(0) = *(a->p = a->b + -- a->l); }
  ISN(dict_new): {
      stacky_dict *d = malloc(sizeof(*d));
      stacky_array_init(Y, (stacky_array*) d, sizeof(stky_v), 8);
      d->a.o.type = stky_t(dict);
      d->eq = V(0);
      // fprintf(stderr, "  dict_new %lld\n", (long long) d);
      Vt(0,stacky_dictP) = d;
    }
  ISN(dict_get): {
      stacky_dict *d = Vt(2,stacky_dictP);
      stky_v k = V(1), v = V(0);
      size_t i = 0;
      while ( i < d->a.l ) {
        PUSH(k); PUSH(d->a.b[i]); PUSH(d->eq); CALLISN(isn_call);
        if ( Vi(0) ) {
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
      stacky_dict *d = Vt(2,stacky_dictP);
      stky_v k = V(1), v = V(0);
      size_t i = 0;
      while ( i < d->a.l ) {
        PUSH(k); PUSH(d->a.b[i]); PUSH(d->eq); CALLISN(isn_call);
        if ( Vi(0) ) {
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
  ISN(sym): {
      stacky_string *str = V(0); stacky_symbol *sym;
      PUSH(Y->sym_dict); PUSH(str); PUSH(0); CALLISN(isn_dict_get);
      if ( V(0) ) {
        V(1) = sym = V(0); POP();
        // fprintf(stderr, "  sym @%p %s \n", sym, str->b);
      } else {
        POP();
        str = (void*) stacky_array_dup(Y, (void*) str);
        str->p[str->l] = 0;
        sym = malloc(sizeof(*sym));
        sym->o.type = stky_t(symbol);
        sym->name = str;
        PUSH(Y->sym_dict); PUSH(str); PUSH(sym); CALLISN(isn_dict_set);
        V(1) = sym; POP();
        // fprintf(stderr, "  sym @%p %s NEW \n", sym, str->b);
      }
    }
  ISN(sym_charP): {
      char *str = (void*) *(pc ++);
      stacky_string *s = stky_string_new_charP(Y, str, -1LL);
      PUSH(s); CALLISN(isn_sym);
      pc[-2] = isn_lit;
      pc[-1] = (stky_i) V(0);
    }
  ISN(dict_stack): PUSH(Y->dict_stack);
  ISN(dict_stack_top): {
      stacky_array *a = (void*) Y->dict_stack;
      PUSH(a->p[a->l - 1]);
    }
  ISN(lookup): {
      stky_v k = V(0);
      int i = Y->dict_stack->l;
      while ( -- i >= 0 ) {
        PUSH(Y->dict_stack->p[i]); PUSH(k); PUSH((stky_v) &k); CALLISN(isn_dict_get);
        if ( V(0) != (stky_v) &k ) {
          V(1) = V(0); POP();
          goto lookup_done;
        }
        POP();
      }
      V(0) = 0; // NOT_FOUND
    lookup_done:
      (void) 0;
    }
  ISN(call):
    *(ep ++) = pc;
    pc = Vt(0,stky_i*); POP();
  ISN(rtn):
    if ( ep <= eb ) goto rtn;
    pc = *(-- ep);
  ISN(Y): PUSH((stky_v) Y);
  ISN(c_proc):
    switch ( *(pc ++) ) {
    case 0:
      ((void (*)()) V(0)) (); POPN(1); break;
    case 1:
      ((void (*)(stky_v)) V(0)) (V(1)); POPN(2); break;
    case 2:
      ((void (*)(stky_v, stky_v)) V(0)) (V(2), V(1)); POPN(3); break;
    default: abort();
    }
  ISN(c_func):
    switch ( *(pc ++) ) {
    case 0:
      V(0) = ((stky_v (*)()) V(0)) (); break;
    case 1:
      V(1) = ((stky_v (*)(stky_v)) V(0)) (V(1)); POPN(1); break;
    case 2:
      V(2) = ((stky_v (*)(stky_v, stky_v)) V(0)) (V(2), V(1)); POPN(2); break;
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

stky_v stacky_pop(stacky *Y)
{
  return *(Y->vs.p --);
}

stacky *stacky_new()
{
  stacky *Y = malloc(sizeof(*Y));
  Y->o.type = stky_t(stacky);
  Y->v_stdin  = stdin;
  Y->v_stdout = stdout;
  Y->v_stderr = stderr;

#define TYPE(NAME,IND)                           \
  Y->types[IND].o.type = stky_t(type);           \
  Y->types[IND].name = #NAME;
#include "stacky/types.h"

  stacky_array_init(Y, &Y->vs, sizeof(stky_v), 1024);
  *(Y->vs.p) = Y;
  stacky_array_init(Y, &Y->es, sizeof(stky_v), 1024);

  {
    char *v; int i;
    if ( (v = getenv("STACKY_TRACE")) && (i = atoi(v)) > 0 )
      Y->trace = i;
  }
  {
    static stky_i e_eq_string[] = { isn_string_eq, isn_rtn, isn_END };
    static stky_i e[] = { isn_lit_voidP, (stky_i) e_eq_string, isn_dict_new, isn_rtn, isn_END }; 
    Y->sym_dict = stacky_pop(stacky_call(Y, e));
  }
  { 
    static stky_i e[] = { isn_lit_int, 0, isn_array_new, isn_rtn, isn_END };
    Y->dict_stack = stacky_pop(stacky_call(Y, e));
  }
  {
    static stky_i e_eq[] = { isn_hdr, isn_eq, isn_rtn, isn_END };
    stky_i e[] = {
      isn_dict_stack,
      isn_lit_voidP, (stky_i) e_eq, isn_dict_new,
      isn_array_push, isn_pop,
      isn_rtn, isn_END };
    stacky_call(Y, e);
  }
  {
    int i;
    for ( i = 0; isn_table[i].name; ++ i ) {
      char name[32] = { 0 };
      stky_i e_isn_[] = { isn_hdr, isn_table[i].isn, isn_rtn, isn_END };
      stky_i *e_isn = memcpy(malloc(sizeof(e_isn_)), e_isn_, sizeof(e_isn_));
      stky_i e[] = {
        isn_dict_stack_top,
        isn_sym_charP, (stky_i) isn_table[i].isn_sym,
        isn_lit_voidP, (stky_i) e_isn,
        isn_dict_set, isn_pop,
        isn_rtn, isn_END };
      // ++ Y->trace;
      stacky_call(Y, e);
      // -- Y->trace;
    }
  }

  return Y;
}

