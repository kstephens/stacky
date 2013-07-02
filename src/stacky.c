#include "stacky/stacky.h"
#include "gc/gc.h"
#include <ctype.h>

static stacky_isn isn_defs[] = {
#define ISN(name,nwords) { { 0 }, -1, isn_##name, nwords, #name, "&&" #name },
#include "stacky/isns.h"
  { 0, 0, 0, 0 },
};

#if 0
#define stky_malloc(S)    GC_malloc(S)
#define stky_realloc(P,S) GC_realloc(P,S)
#else
void *stky_malloc_(size_t s, const char *file, int line)
{
  // if ( s > 1000000 ) abort();
  void *p = GC_malloc(s);
  // fprintf(stderr, "  stky_malloc(%lu) => %p     \t %s:%d\n", (unsigned long) s, p, file, line);
  return p;
}
void *stky_realloc_(void *o, size_t s, const char *file, int line)
{
  // if ( s > 1000000 ) abort();
  void *p = GC_realloc(o, s);
  // fprintf(stderr, "  stky_realloc(%p, %lu) => %p \t %s:%d\n", o, (unsigned long) s, p, file, line);
  return p;
}
#define stky_malloc(S)    stky_malloc_(S, __FILE__, __LINE__)
#define stky_realloc(P,S) stky_realloc_(P,S, __FILE__, __LINE__)
#endif

stky_v stky_object_init(stacky *y, void *p, stacky_type *type, size_t size)
{
  stacky_object *o = p;
  o->type = type;
  o->flags = 0;
  o->size = size;
  return o;
}

stky_v stky_object_new(stacky *Y, stacky_type *type, size_t size)
{
  stacky_object *o = stky_malloc(size);
  stky_object_init(Y, o, type, size);
  return o;
}

stky_v stky_object_dup(stacky *Y, stky_v v)
{
  stacky_object *a = v;
  stacky_object *b = stky_malloc(a->size);
  *b = *a;
  return b;
}

stky_v stky_literal_new(stacky *Y, stky_v v)
{
  stacky_literal *o = stky_object_new(Y, stky_t(literal), sizeof(*o));
  o->value = v;
  return o;
}

stacky_bytes *stacky_bytes_init(stacky *Y, stacky_bytes *a, size_t es, size_t s)
{
  size_t cs;
  a->p = a->b = stky_malloc(cs = (a->es = es) * ((a->s = s) + 1));
  memset(a->b, 0, cs);
  a->l = 0;
  return a;
}

stacky_bytes *stacky_bytes_dup(stacky *Y, stacky_bytes *a)
{
  stacky_bytes *b = stky_object_dup(Y, a);
  *b = *a;
  stacky_bytes_init(Y, b, a->es, a->s);
  memcpy(b->b, a->b, a->es * (a->s + 1));
  return a;
}

stacky *stacky_bytes_resize(stacky *Y, stacky_bytes *a, size_t s)
{
  stky_v *nb;
  // fprintf(stderr, "  array %p b:s %p:%lu p:l %p:%lu\n", a, a->b, (unsigned long) a->s, a->p, (unsigned long) a->l);  
  nb = stky_realloc(a->b, a->es * (s + 1));
  a->s = s;
  if ( a->l > a->s ) a->l = a->s;
  a->p = nb + (a->p - a->b);
  a->b = nb;
  void *e = a->b + a->es * a->s;
  if ( a->p > e ) a->p = e;
  // fprintf(stderr, "  array %p b:s %p:%lu p:l %p:%lu\n\n", a, a->b, (unsigned long) a->s, a->p, (unsigned long) a->l);  
  return Y;
}

stacky *stacky_bytes_append(stacky *Y, stacky_bytes *a, const void *p, size_t s)
{
  if ( a->l + s > a->s ) stacky_bytes_resize(Y, a, a->l + s);
  memmove(((void*) a->p) + a->es * a->l, p, a->es * s);
  a->l += s;
  memset( ((void*) a->p) + a->es * a->l, 0, a->es);
  return Y;
}

stacky_array *stacky_array_init(stacky *Y, stacky_array *a, size_t s)
{
  stacky_bytes_init(Y, (void*) a, sizeof(a->p[0]), s);
  return a;
}

stacky_words *stacky_words_init(stacky *Y, stacky_words *a, size_t s)
{
  stacky_bytes_init(Y, (void*) a, sizeof(a->p[0]), s);
  return a;
}

stacky_words *stacky_words_new(stacky *Y, const stky_i *p, size_t s)
{
  stacky_words *o = stky_object_new(Y, stky_t(words), sizeof(*o));
  stacky_words_init(Y, (void*) o, s);
  if ( p ) memcpy(o->p, p, sizeof(o->p[0]) * s);
  o->l = s;
  o->o.flags |= 1;
  o->name = 0;
  return o;
}

stacky_string *stky_string_new_charP(stacky *Y, const char *p, size_t s)
{
  stacky_string *o;
  if ( s == (size_t) -1 ) s = strlen(p);
  o = stky_object_new(Y, stky_t(string), sizeof(*o));
  stacky_bytes_init(Y, (void*) o, 1, s);
  o->o.type = stky_t(string);
  if ( p ) memcpy(o->b, p, s);
  o->l = s;
  return o;
}

stacky *stky_string_append(stacky *Y, stacky_string *o, const void *p, size_t s)
{
  stacky_array *a = (void*) o;
  if ( s == (size_t) -1 ) s = strlen(p);
  return stacky_bytes_append(Y, (void*) a, p, s);
}

stacky *stky_string_append_char(stacky *Y, stacky_string *o, int c)
{
  char p[] = { c };
  return stky_string_append(Y, o, p, 1);
}

stky_v stky_array_top(stacky *Y, stacky_array *o)
{
  if ( o->l <= 0 ) abort();
  return o->p[o->l - 1];
}
stky_v stky_array_top_(stacky *Y, stacky_array *o, stky_v v)
{
  if ( o->l <= 0 ) abort();
  return o->p[o->l - 1] = v;
}
stky_v* stky_array_popn(stacky *Y, stacky_array *o, size_t s)
{
  if ( o->l < s ) abort();
  return o->p + (o->l -= s);
}
stky_v stky_array_pop(stacky *Y, stacky_array *o)
{
  return *stky_array_popn(Y, o, 1);
}
stky_v stky_array_push(stacky *Y, stacky_array *o, stky_v v)
{
  if ( o->l >= o->s ) stacky_bytes_resize(Y, (void*) o, o->s + 1024);
  return o->p[o->l ++] = v;
}

stky_v stky_top(stacky *Y) { return stky_array_top(Y, &Y->vs); }
stky_v stky_top_(stacky *Y, stky_v v) { return stky_array_top_(Y, &Y->vs, v); }
stky_v stky_push(stacky *Y, stky_v v) { 
  if ( Y->trace > 0 ) { fprintf(stderr, "  # << "); stky_write(Y, v, stderr, 1); fprintf(stderr, "\n"); }
  return stky_array_push(Y, &Y->vs, v); }
stky_v stky_pop(stacky *Y) { 
  if ( Y->trace > 0 ) { fprintf(stderr, "  # >> "); stky_write(Y, stky_top(Y), stderr, 1); fprintf(stderr, "\n"); }
  return stky_array_pop(Y, &Y->vs); }
stky_v *stky_popn(stacky *Y, size_t s ) {
  if ( Y->trace > 0 ) { fprintf(stderr, "  # << # %lu\n", s); }
  return stky_array_popn(Y, &Y->vs, s); }

static
void stky_print_vs(stacky *Y, FILE *out)
{
  size_t i = 0;
  fprintf(out, "  # V: ");
  while ( i < Y->vs.l ) {
    stky_write(Y, Y->vs.p[i ++], out, 2);
    fprintf(out, " ");
  }
  fprintf(out, "| \n");
}

stacky *stacky_call(stacky *Y, stky_i *pc)
{
  size_t es_i = Y->es.l;
  stky_v val;

#define VS_SAVE()
#define VS_RESTORE()

#define PUSH(X) stky_push(Y, (X))
#define PUSHt(X,T) PUSH((stky_v) (X))
#define POP() stky_pop(Y)
#define POPN(N) stky_popn(Y, (N))
#define vp (Y->vs.p + (Y->vs.l - 1))
#define V(i) vp[- (i)]
#define Vt(i,t) (*((t*) (vp - (i))))
#define Vi(i) stky_v_int_(V(i))
#define CALL(P) do {                               \
    stky_array_push(Y, &Y->es, (stky_v) pc);       \
    pc = (stky_i*) (P);                            \
    goto call;                                     \
  } while (0)
#define CALLISN(I) do {                         \
    if ( 0 ) { fprintf(stderr, "  # CALLISN(%s)\n", #I); }      \
    Y->trace --;                                \
    stacky_call(Y, Y->isns[I].words->p);        \
    Y->trace ++;                                \
  } while (0)

 call:
  if ( Y->trace > 0 ) { fprintf(stderr, "  # {\n"); }
#if 0
  if ( ! Y->isns[0].addr ) {
    struct isn_def *isn;
#define ISN(name,lits) isn = &Y->isns[isn_##name]; isn->addr = &&L_##name;
#include "stacky/isns.h"
    Y->isns[0].addr = (void*) i;
  }

  if ( Y->threaded_comp && *pc == isn_hdr ) {
    stky_i *pc_save = pc;
    *(pc ++) = isn_hdr_;
    while ( *pc != isn_END ) {
      if ( ! stky_v_isnQ(*pc) ) { pc ++; continue; }
      struct isn_def *isn = &Y->isns[*pc];
      fprintf(stderr, "  %4d %10s => @%p\n", (int) (pc - pc_save), isn->name, isn->addr);
      *pc = (stky_i) isn->addr;
      pc += isn->nwords;
    }
    *pc = (stky_i) &abort;
    pc = pc_save;
    pc ++;
    goto *((void**) (pc ++));
  }
#endif

 next_isn:
#if 1
  if ( Y->trace > 0 ) {
    { 
      stky_i **p = (stky_i **) Y->es.p;
      fprintf(stderr, "\n  # E: ");
      while ( p < (stky_i **) (Y->es.p + Y->es.l) )
        fprintf(stderr, "@%p ", (void*) *(p ++));
      fprintf(stderr, "| \n");
    }
    stky_print_vs(Y, stderr);
    switch ( stky_v_type_i(*pc) ) {
    case stky_t_isn:
    if ( *pc != isn_END ) {
      fprintf(stderr, "  # I: %-20s @%p", Y->isns[*pc].name, (void*) pc);
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
    break;
    default:
      fprintf(stderr, "  # <: ");
      stky_write(Y, (stky_v) *pc, stderr, 1); 
    }
    fprintf(stderr, "\n");
  }
#endif

  val = (stky_v) *(pc ++);
 eval:
#define TYPE(name) goto next_isn; case stky_t_##name: T_##name
  switch ( stky_v_type_i(val) ) {
  default:       PUSH(val);
  TYPE(isn):     goto do_isn;
  TYPE(string):  PUSH(val);
  TYPE(literal): PUSH(((stacky_literal*) val)->value);
  TYPE(symbol):  stky_exec(Y, isn_lookup, isn_eval);
  TYPE(array):
    if ( ((stacky_object*) val)->flags & 1 ) {
    call_array:
      CALL(((stacky_array*) val)->p);
    } else {
      PUSH(val);
    }
  TYPE(words): 
    // fprintf(stderr, "  # words @%p ", val); stky_write(Y, val, stderr, 2); fprintf(stderr, "\n");
    goto call_array;
  }
#undef TYPE
  }

  do_isn:
#define ISN(name) goto next_isn; case isn_##name: L_##name
  switch ( (int) val ) {
  case 0: goto L_rtn;
// #include "isns.c"
  ISN(nul):   abort();
  ISN(hdr):
  ISN(hdr_):
  ISN(nop):
  ISN(lit):       PUSH((stky_v) *(pc ++));
  ISN(lit_int):   PUSH(stky_v_int(*(pc ++)));
  ISN(lit_charP): { stky_v v = stky_string_new_charP(Y, (void*) *(pc ++), -1);
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
  ISN(rotl): { 
      size_t n = Vi(0); POP();
      // 3 2 1 0 n | 2 1 0 3 : n = 4
      //       ^
      stky_v tmp = V(n - 1);
      memmove(&V(n - 1), &V(n - 2), sizeof(V(0)) * (n - 1));
      V(0) = tmp;
    }
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
  ISN(write):
    stky_write(Y, V(1), Vt(0,FILE*), 9999); POPN(2);
  ISN(write_int):
    fprintf(Vt(0,FILE*), "%lld", (long long) Vi(1)); POPN(2);
  ISN(write_char):
    fprintf(Vt(0,FILE*), "%c", (int) stky_v_char_(V(1))); POPN(2);
  ISN(write_symbol): {
      stacky_symbol *s = Vt(1,stacky_symbolP); 
      V(1) = s->name; goto L_write_string; }
  ISN(write_string): { 
      stacky_string *s = Vt(1,stacky_stringP); 
      fwrite(s->b, 1, s->l, Vt(0,FILE*)); (void) POPN(2); }
  ISN(write_voidP):
    fprintf(Vt(0,FILE*), "@%p", Vt(1,voidP)); POPN(2);
  ISN(c_malloc):  Vt(0,voidP) = malloc(Vi(0));
  ISN(c_realloc): Vt(1,voidP) = realloc(Vt(1,voidP), Vi(0)); POP();
  ISN(c_free):    free(Vt(0,voidP)); POP();
  ISN(c_memmove): memmove(Vt(2,voidP), Vt(1,voidP), Vi(0)); POPN(3);
  ISN(v_tag):     V(0) = stky_v_int(stky_v_tag(V(0)));
  ISN(v_type):    V(0) = stky_v_type(V(0));
  ISN(cve):       Vt(0, stacky_object*)->flags |= 1;
  ISN(eval):      val = POP(); goto eval;
  ISN(mark):      PUSH(stky_v_mark);
  ISN(marke):     PUSH(stky_v_marke); Y->defer_eval ++;
  ISN(ctm): {
      size_t n = 0; stky_v *p = vp;
      while ( p >= Y->vs.p ) {
        if ( *(p --) == stky_v_mark ) break;
        ++ n;
      }
      PUSH(stky_v_int(n));
    }
  ISN(array_stk): {
      size_t as = stky_v_int_(POP());
      stacky_array *a = stacky_array_init(Y, stky_object_new(Y, stky_t(array), sizeof(*a)), as);
      fprintf(stderr, "   %lu array_stk => @%p\n", as, a);
      memmove(a->p, POPN(a->l = as), sizeof(a->p[0]) * as);
      V(0) = a;
    }
  ISN(array_new):
      Vt(0,stacky_arrayP) =
        stacky_array_init(Y, stky_object_new(Y, stky_t(array), sizeof(stacky_array)), Vi(0));
  ISN(array_tm):
      stky_exec(Y, isn_ctm, isn_array_stk, isn_swap, isn_pop);
  ISN(array_tme):
      PUSH((stky_v) isn_rtn);
      stky_exec(Y, isn_ctm, isn_array_stk, isn_swap, isn_pop, isn_cve);
  ISN(array_b):  V(0) = Vt(0,stacky_arrayP)->b;
  ISN(array_p):  V(0) = Vt(0,stacky_arrayP)->p;
  ISN(array_l):  V(0) = stky_v_int(Vt(0,stacky_arrayP)->l);
  ISN(array_s):  V(0) = stky_v_int(Vt(0,stacky_arrayP)->s);
  ISN(array_es): V(0) = stky_v_int(Vt(0,stacky_arrayP)->es);
  ISN(array_get):  V(1) = Vt(1,stacky_arrayP)->b[Vi(0)]; POP();
  ISN(array_set):  Vt(2,stacky_arrayP)->b[Vi(1)] = V(0); POPN(2);
  ISN(array_push):
      stky_array_push(Y, V(1), V(0));
      POP();
  ISN(array_pop):
      V(0) = stky_array_pop(Y, V(0));
  ISN(dict_new): {
      stacky_dict *d = stky_object_new(Y, stky_t(dict), sizeof(*d));
      stacky_array_init(Y, (stacky_array*) d, 8);
      d->eq = V(0);
      // fprintf(stderr, "  dict_new %lld\n", (long long) d);
      Vt(0,stacky_dictP) = d;
    }
  ISN(dict_get): { // dict k d DICT_GET | (v|d)
      stacky_dict *d = Vt(2,stacky_dictP);
      stky_v k = V(1), v = V(0);
      size_t i = 0;
      while ( i < d->a.l ) {
        PUSH(k); PUSH(d->a.p[i]); PUSH(d->eq); CALLISN(isn_call);
        if ( Vi(0) ) {
          POP();
          V(2) = d->a.p[i + 1];
          goto dict_get_done;
        }
        POP();
        i += 2;
      }
      V(2) = v;
    dict_get_done:
      POPN(2);
    }
  ISN(dict_set): { // dict k v DICT_SET |
      stacky_dict *d = Vt(2,stacky_dictP);
      stky_v k = V(1), v = V(0);
      size_t i = 0;
      while ( i < d->a.l ) {
        PUSH(k); PUSH(d->a.p[i]); PUSH(d->eq); CALLISN(isn_call);
        if ( Vi(0) ) {
          POP();
          d->a.p[i + 1] = V(0);
          (void) POPN(2);
          goto dict_set_done;
        }
        POP();
        i += 2;
      }
      POPN(2);
      PUSH(k); CALLISN(isn_array_push);
      PUSH(v); CALLISN(isn_array_push);
    dict_set_done:
      if ( d != Y->sym_dict ) {
        fprintf(stderr, "  dict_set => "); stky_write(Y, d, stderr, 3); fprintf(stderr, "\n");
      }
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
        str = (void*) stacky_bytes_dup(Y, (void*) str);
        sym = stky_object_new(Y, stky_t(symbol), sizeof(*sym));
        sym->name = str;
        PUSH(Y->sym_dict); PUSH(str); PUSH(sym); CALLISN(isn_dict_set);
        V(1) = sym; POP();
        // fprintf(stderr, "  sym @%p %s NEW \n", sym, str->b);
      }
    }
  ISN(sym_charP): {
      char *str = (void*) *(pc ++);
      stacky_string *s = stky_string_new_charP(Y, str, -1);
      PUSH(s); CALLISN(isn_sym);
      pc[-2] = isn_lit;
      pc[-1] = (stky_i) V(0);
    }
  ISN(dict_stack): PUSH(Y->dict_stack);
  ISN(dict_stack_top):
      PUSH(stky_array_top(Y, Y->dict_stack));
  ISN(lookup): {
      stky_v k = V(0);
      int i = Y->dict_stack->l;
      while ( -- i >= 0 ) {
        PUSH(Y->dict_stack->p[i]); PUSH(k); PUSH(Y->v_lookup_na); CALLISN(isn_dict_get);
        if ( V(0) != Y->v_lookup_na ) {
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
    stky_array_push(Y, &Y->es, pc);
    val = POP();
    goto eval;
  ISN(rtn):
    if ( Y->es.l <= es_i ) goto rtn;
    pc = stky_array_pop(Y, &Y->es);
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
  if ( Y->trace > 0 ) { fprintf(stderr, "  # }\n"); }
  return Y;
}
#undef vp

static
const char *char_to_str(int c)
{
  static char buf[32][8];
  static int buf_i = 0;
  char *s = buf[buf_i = (buf_i + 1) % 32];
  sprintf(s, c > 32 && isprint(c) ? "'%c'" :  "'\\0%o'", c);
  return s;
}

enum read_token_state {
    s_error = -2,
    s_eos = -1,
    s_stop = 0,
    s_start,
    s_comment_eol,
    s_symbol,
    s_literal,
    s_pot_num,
    s_int,
    s_char,
    s_string,
};

stky_v stky_read_token(stacky *Y, FILE *in)
{
  stky_v value;
  stacky_string *token = 0;
  stky_si n = 0;
  int pot_num_c = 0;
  int literal = 0;
  int state = s_start, last_state = -1, last_state_2;
  int c = -2;

#define take_c() c = -2
#define next_c() take_c(); goto again
#define next_s(s) state = (s); goto again
#define stop_s(s) last_state_2 = (s); next_s(s_stop)
 again:
  last_state = last_state_2; last_state_2 = state;
  if ( ! token ) token = stky_string_new_charP(Y, 0, 0);
  if ( c == -2 ) c = fgetc(in);
  // fprintf(stderr, "  c = %s, state = %d, token = '%s'\n", char_to_str(c), state, token->p);
  switch ( state ) {
  case s_error:
    stky_string_append_char(Y, token, c);
    value = token;
  case s_eos:
  case s_stop:
    break;
  case s_start:
    switch ( c ) {
    case EOF: stop_s(s_eos);
    case ' ': case '\t': case '\n': case '\r':
      next_c();
    case '#': take_c(); next_s(s_comment_eol);
    case '/':
      literal ++; next_c();
    case '-': case '+':
      stky_string_append_char(Y, token, c);
      pot_num_c = c; take_c();
      next_s(s_pot_num);
    case '0' ... '9':
      next_s(s_int);
    case '\'':
      state = s_char; next_c();
    case '"':
      state = s_string; next_c();
    case ',':
      fprintf(stderr, "\n");
      stky_print_vs(Y, stderr);
      fprintf(stderr, "\n");
      next_c();
    default:
      next_s(s_symbol);
    }
  case s_comment_eol:
    switch ( c ) {
    case '\n':
      take_c(); next_s(s_start);
    case EOF:
      next_s(s_start);
    default:
      next_c();
    }
  case s_symbol:
    switch ( c ) {
    case EOF:
    case ' ': case '\t': case '\n': case '\r':
      stky_push(Y, token);
      stky_exec(Y, isn_sym);
      value = stky_pop(Y); next_s(s_stop);
    default:
      stky_string_append_char(Y, token, c);
      next_c();
    }
  case s_pot_num:
    switch ( c ) {
    case '0' ... '9':
      next_s(s_int);
    case EOF:
      next_s(s_int);
    case ' ': case '\t': case '\n': case '\r':
      value = token; next_s(s_stop);
    default:
      next_s(s_symbol);
    }
  case s_int:
    switch ( c ) {
    case '0' ... '9':
      stky_string_append_char(Y, token, c); take_c();
      {
        stky_si prev_n = n;
        n = n * 10 + c - '0';
        if ( n < prev_n ) { next_s(s_symbol); }
      }
      next_c();
    case EOF:
    case ' ': case '\t': case '\n': case '\r':
      if ( pot_num_c == '-' ) n = - n;
      value = stky_v_int(n); next_s(s_stop);
    default:
      next_s(s_symbol);
    }
  case s_char:
    switch ( c ) {
    default:
      value = stky_v_char(c); next_s(s_stop);
    }
  case s_string:
    switch ( c ) {
    case '"':
      value = token; take_c(); next_s(s_stop);
    default:
      stky_string_append_char(Y, token, c);
      next_c();
    }
  default:
    abort();
  }
#undef take_c
#undef next_c
#undef next_s
#undef stop_s
 stop:
  if ( c >= 0 ) ungetc(c, in);
  while ( literal -- ) {
    value = stky_literal_new(Y, value);
    last_state = s_literal;
  }
  stky_push(Y, value);
  stky_push(Y, stky_v_int(last_state));

  fprintf(stderr, "  : ");
  stky_write(Y, value, stderr, 1);
  fprintf(stderr, " %d\n", last_state);
  stky_print_vs(Y, stderr);

  return Y;
}

stacky *stky_repl(stacky *Y, FILE *in, FILE *out)
{
  int c = 0;
  while ( ! feof(in) ) {
    stky_read_token(Y, in);
    switch ( stky_v_int_(stky_pop(Y)) ) {
    case s_eos:
      break;
    default:
      stky_exec(Y, isn_eval);
    }
    fprintf(stderr, "  =>");
    stky_print_vs(Y, stderr);
    fprintf(stderr, "\n");
  }
  return Y;
}

stacky *stky_write(stacky *Y, stky_v v, FILE *out, int depth)
{
  int is_dict;
 again:
  if ( depth < 1 ) {
    fprintf(out, "@?"); goto rtn;
  }
  is_dict = 0;
  switch ( stky_v_type_i(v) ) {
  case stky_t_null:
    fprintf(out, "@x0:$null"); break;
  case stky_t_mark:
    fprintf(out, "%s", ((stacky_string*) v)->p); break;
  case stky_t_int:
    fprintf(out, "%lld", (long long) stky_v_int_(v)); break;
  case stky_t_char:
    fprintf(out, "%s", char_to_str(stky_v_char_(v))); break;
  case stky_t_type:
    fprintf(out, "$%s", stky_v_type(v)->name); break;
  case stky_t_isn:
    fprintf(out, "%s", Y->isns[(stky_i) v].sym_name); break;
  case stky_t_string:
    fprintf(out, "\"%s\"", ((stacky_string*) v)->p); break;
  case stky_t_words:
    if ( ((stacky_words *) v)->name ) {
      fprintf(out, "%s:$words", ((stacky_words *) v)->name); break;
    }
    goto array;
  case stky_t_dict:
    is_dict = 1;
  case stky_t_array:
  array:
    if ( depth < 2 ) goto shallow;
    {
    stacky_array *a = v;
    fprintf(out, "@%p:", v);
    if ( is_dict ) fprintf(out, "(");
    fprintf(out, a->o.flags & 1 ? "{ " : "[ ");
    for ( size_t i = 0; i < a->l; ++ i ) {
      stky_write(Y, a->p[i], out, depth - 1);
      fprintf(out, " ");
    }
    fprintf(out, a->o.flags & 1 ? "} " : "] ");
    if ( is_dict ) {
      stky_write(Y, ((stacky_dict*) v)->eq, out, depth - 1);
      fprintf(out, " dict)");
    }
  } break;
  case stky_t_symbol:
    fwrite(((stacky_symbol*) v)->name->p, ((stacky_symbol*) v)->name->l, 1, out);
    break;
  case stky_t_literal:
    fprintf(out, "/"); v = ((stacky_literal*) v)->value; goto again;
  default:
  shallow:
    fprintf(out, "@%p:$%s", v, stky_v_type(v)->name); break;
    abort();
  }
 rtn:
  return Y;
}

stacky *stacky_new()
{
  stacky *Y = 0;
  int i;

  GC_init();
  Y = stky_object_new(Y, stky_t(stacky), sizeof(*Y));
  Y->o.type = stky_t(stacky);
  Y->v_stdin  = stdin;
  Y->v_stdout = stdout;
  Y->v_stderr = stderr;

  Y->v_mark =  (void*) stky_string_new_charP(Y, "[", 1);
  Y->v_marke = (void*) stky_string_new_charP(Y, "{", 1);
  ((stacky_object *) Y->v_mark)->type  = stky_t(mark);
  ((stacky_object *) Y->v_marke)->type = stky_t(mark);

  Y->v_lookup_na = (void*) stky_string_new_charP(Y, "&&lookup_na", -1);
  ((stacky_object *) Y->v_lookup_na)->type = stky_t(mark);

  {
    stacky_type *t;
#define TYPE(NAME)                                              \
    t = &Y->types[stky_t_##NAME];                               \
    stky_object_init(Y, t, stky_t(type), sizeof(*t));           \
    t->i = stky_t_##NAME;                                       \
    t->name = #NAME;
#include "stacky/types.h"
  }

  stky_object_init(Y, &Y->vs, stky_t(array), sizeof(Y->vs));
  stacky_array_init(Y, &Y->vs, 1023);
  Y->vs.p[Y->vs.l ++] = Y;

  stky_object_init(Y, &Y->es, stky_t(array), sizeof(Y->es));
  stacky_array_init(Y, &Y->es, 1023);
  Y->es.p[Y->es.l ++] = Y;

  {
    char *v;
    if ( (v = getenv("STACKY_TRACE")) && (i = atoi(v)) > 0 )
      Y->trace = i;
  }

  for ( i = 0; isn_defs[i].name; ++ i ) {
    stky_i isn = isn_defs[i].isn;
    stacky_words *isn_w;

    isn_defs[i].i = i;
    Y->isns[isn] = isn_defs[i];
    // Y->isns[isn].o.type = stky_t(isn);
    isn_w = Y->isns[isn].words = stky_words(Y, isn, isn_rtn);
    isn_w->name = Y->isns[isn].sym_name + 1;
  }

  Y->sym_dict = stky_pop(stky_exec(Y, isn_lit, stky_isn_w(isn_string_eq), isn_dict_new));
  Y->dict_stack = stky_pop(stky_exec(Y, isn_lit_int, 0, isn_array_new));
  stky_exec(Y,
            isn_dict_stack,
            isn_lit, stky_isn_w(isn_eq), isn_dict_new,
            isn_array_push, isn_pop);
  for ( i = 0; isn_defs[i].name; ++ i ) {
    stky_i isn = isn_defs[i].isn;
    stky_v isn_w = Y->isns[isn].words;
    // fprintf(stderr, "  isn %ld => ", isn); stky_write(Y, isn_w, stderr, 2); fprintf(stderr, "\n");
    stky_exec(Y,
              isn_dict_stack_top,
              isn_sym_charP, (stky_i) Y->isns[isn].sym_name + 1,
              isn_lit, (stky_i) isn_w,
              isn_dict_set, isn_pop,
              isn_dict_stack_top,
              isn_sym_charP, (stky_i) Y->isns[isn].sym_name,
              isn_lit, (stky_i) isn,
              isn_dict_set, isn_pop);
    // -- Y->trace;
  }

  stky_exec(Y,
            isn_dict_stack_top, isn_sym_charP, (stky_i) "[", isn_lit, stky_isn_w(isn_mark),  isn_dict_set, isn_pop,
            isn_dict_stack_top, isn_sym_charP, (stky_i) "{", isn_lit, stky_isn_w(isn_marke), isn_dict_set, isn_pop,
            isn_dict_stack_top, isn_sym_charP, (stky_i) "]", isn_lit, stky_isn_w(isn_array_tm),  isn_dict_set, isn_pop,
            isn_dict_stack_top, isn_sym_charP, (stky_i) "}", isn_lit, stky_isn_w(isn_array_tme), isn_dict_set, isn_pop);
            
  fprintf(stderr, "\n\n dict_stack:\n");
  stky_write(Y, Y->dict_stack, stderr, 9999);
  fprintf(stderr, "\n\n");

  {
    FILE *fp;

    fprintf(stderr, "  # reading boot.stky\n");
    if ( (fp = fopen("boot.stky", "r")) ) {
      stky_repl(Y, fp, 0);
      fclose(fp);
    } else {
      perror("cannot read boot.stky");
    }
  }

  fprintf(stderr, "\n\n dict_stack:\n");
  stky_write(Y, Y->dict_stack, stderr, 9999);
  fprintf(stderr, "\n\n");

  return Y;
}

