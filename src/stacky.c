#include "stacky/stacky.h"
#include "gc/gc.h"
#include <ctype.h>

struct isn_def {
  stky_i isn;
  int nwords;
  const char *name;
  const char *isn_sym;
  void *addr;
};

static struct isn_def isn_table[] = {
#define ISN(name,nwords) { isn_##name, nwords, #name, "&&" #name },
#include "stacky/isns.h"
  { 0, 0, 0 },
};

#if 0
#define stky_malloc(S)    GC_malloc(S)
#define stky_realloc(P,S) GC_realloc(P,S)
#else
void *stky_malloc_(size_t s, const char *file, int line)
{
  if ( s > 1000000 ) abort();
  void *p = GC_malloc(s);
  // fprintf(stderr, "  stky_malloc(%lu) => %p     \t %s:%d\n", (unsigned long) s, p, file, line);
  return p;
}
void *stky_realloc_(void *o, size_t s, const char *file, int line)
{
  if ( s > 1000000 ) abort();
  void *p = GC_realloc(o, s);
  // fprintf(stderr, "  stky_realloc(%p, %lu) => %p \t %s:%d\n", o, (unsigned long) s, p, file, line);
  return p;
}
#define stky_malloc(S)    stky_malloc_(S, __FILE__, __LINE__)
#define stky_realloc(P,S) stky_realloc_(P,S, __FILE__, __LINE__)
#endif

#define stky_exec(Y,ISNS...)                                            \
  ({ stky_i _isns_##__LINE__[] = { ISNS, isn_rtn, isn_END }; stacky_call((Y), _isns_##__LINE__); })

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
  return o;
}
#define stky_words(Y, WORDS...) \
  ({ stky_i _words_##__LINE__[] = { isn_hdr, WORDS, isn_END }; stacky_words_new(Y, _words_##__LINE__, sizeof(_words_##__LINE__)); })

stacky_string *stky_string_new_charP(stacky *Y, char *p, size_t s)
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

stky_v stky_push(stacky *Y, stky_v v)
{
  stky_v  *vp = Y->vs.p,  *ve = Y->vs.b + Y->vs.s;

#define PUSH(X) do {                                                  \
    stky_v __tmp = (stky_v) (X);                                      \
    if ( ++ vp >= ve ) {                                              \
      Y->vs.p = vp;                                                   \
      stacky_bytes_resize(Y, (void*) &Y->vs, Y->vs.s + 1024);         \
      vp = Y->vs.p; ve = Y->vs.b + Y->vs.s;                           \
    }                                                                 \
    *vp = __tmp;                                                      \
  } while(0)

  PUSH(v);
  Y->vs.p = vp;
  return v;
}

stky_v stky_pop(stacky *Y) { return *(Y->vs.p --); }
stky_v stky_top(stacky *Y) { return *(Y->vs.p); }
stky_v stky_top_(stacky *Y, stky_v v) { return *(Y->vs.p) = v; }

stacky *stky_write(stacky *Y, stky_v v, FILE *out);

stacky *stacky_call(stacky *Y, stky_i *pc)
{
  stky_v  *vp = Y->vs.p,  *ve = Y->vs.b + Y->vs.s;
  stky_i **ep = (void*) Y->es.p, **eb = (void*) Y->es.b;

#define PUSHt(X,T) PUSH(X)
#define POP() -- vp
#define POPN(N) vp -= (N)
#define V(i) vp[- (i)]
#define Vt(i,t) (*((t*) (vp - (i))))
#define Vi(i) stky_v_int_(V(i))
#define CALLISN(I) do {                           \
    -- Y->trace;                                  \
    Y->vs.p = vp;                                 \
    stky_exec(Y, (I));                           \
    vp = Y->vs.p; ve = Y->vs.b + Y->vs.s;         \
    ++ Y->trace;                                  \
  } while (0)
#define CALL(E) do {                            \
    Y->vs.p = vp;                                 \
    stacky_call(Y, (E));                        \
    vp = Y->vs.p; ve = Y->vs.b + Y->vs.s;         \
  } while (0)

#if 0
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
#endif

 next_isn:
#if 1
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
      fprintf(stderr, "  # I: %s ", isn_table[stky_v_int_(*pc)].name);
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
#endif

#define ISN(name) goto next_isn; case isn_##name: L_##name
  switch ( (int) *(pc ++) ) {
// #include "isns.c"
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
  ISN(write):
    stky_write(Y, V(0), Vt(0,FILE*)); POPN(2);
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
  ISN(cve):       Vt(0, stacky_object*)->flags |= 1;
  ISN(eval):
    if ( stky_v_type(V(0))->i == stky_t_literal ) {
      V(0) = Vt(0, stacky_literal*)->value;
    } else if ( stky_v_type(V(0))->i == stky_t_symbol ) {
      stky_exec(Y, isn_lookup, isn_eval);
    } else if ( stky_v_type(V(0))->i == stky_t_array && Vt(0, stacky_object*)->flags & 1 ) {
      stacky_array *a = (void*) POP();
      size_t i = 0;
      while ( i ++ < a->l ) {
        PUSH(a->p[i]);
        stky_exec(Y, isn_eval);
      }
    } else if (stky_v_type(V(0))->i == stky_t_words && Vt(0, stacky_object*)->flags & 1 ) {
      stacky_array *a = (void*) POP();
      CALL((void*) a->p);
    }
  ISN(mark):      PUSH(stky_v_mark);
  ISN(ctm): {
      size_t n = 0;
      stky_v *p = vp;
      while ( p > Y->vs.b ) {
        if ( *(p --) == stky_v_mark ) break;
        ++ n;
      }
      PUSH(stky_v_int(n));
    }
  ISN(array_stk): {
      stacky_array *a = 
        stacky_array_init(Y, stky_object_new(Y, stky_t(array), sizeof(*a)), Vi(0));
      memmove(a->p, POPN(a->l), a->es * a->l);
      V(0) = a;
    }
  ISN(array_new):
      Vt(0,stacky_arrayP) =
        stacky_array_init(Y, stky_object_new(Y, stky_t(array), sizeof(stacky_array)), Vi(0));
  ISN(array_b):  V(0) = Vt(0,stacky_arrayP)->b;
  ISN(array_p):  V(0) = Vt(0,stacky_arrayP)->p;
  ISN(array_l):  V(0) = stky_v_int(Vt(0,stacky_arrayP)->l);
  ISN(array_s):  V(0) = stky_v_int(Vt(0,stacky_arrayP)->s);
  ISN(array_es): V(0) = stky_v_int(Vt(0,stacky_arrayP)->es);
  ISN(array_get):  V(1) = Vt(1,stacky_arrayP)->b[Vi(0)]; POP();
  ISN(array_set):  Vt(2,stacky_arrayP)->b[Vi(1)] = V(0); POPN(2);
  ISN(array_push): {
      stacky_array *a = Vt(1,stacky_arrayP);
      if ( a->l >= a->s ) stacky_bytes_resize(Y, (void*) a, a->l + 1);
      *(a->p = a->b + a->l ++) = V(0);
      POP(); }
  ISN(array_pop): {
      stacky_array *a = Vt(0,stacky_array*);
      V(0) = *(a->p = a->b + -- a->l); }
  ISN(dict_new): {
      stacky_dict *d = stky_object_new(Y, stky_t(dict), sizeof(*d));
      stacky_array_init(Y, (stacky_array*) d, 8);
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
        str = (void*) stacky_bytes_dup(Y, (void*) str);
        str->p[str->l] = 0;
        sym = stky_object_new(Y, stky_t(symbol), sizeof(*sym));
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
  ISN(vs): PUSH(&Y->vs);
  ISN(es): PUSH(&Y->es);
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

enum {
    s_error = -2,
    s_eos = -1,
    s_stop = 0,
    s_start,
    s_comment_eol,
    s_word,
    s_word_lit,
    s_pot_num,
    s_num,
    s_char,
    s_str,
    s_stk_op,
};

static
const char *char_to_str(int c)
{
  static char buf[32][8];
  static int buf_i = 0;
  char *s = buf[buf_i = (buf_i + 1) % 32];
  sprintf(s, c > 32 && isprint(c) ? "'%c'" :  "'\\0%o'", c);
  return s;
}

stky_v stky_read_token(stacky *Y, FILE *in)
{
  stky_v value;
  stacky_string *token = 0;
  int n = 0;
  int pot_num_c = 0;
  int word_lit = 0;
  int state = s_start, last_state = -1, last_state_2;
  int c = -2;

#define next_c() c = -2; goto again
#define next_s(s) state = (s); goto again
 again:
  last_state = last_state_2; last_state_2 = state;
  if ( ! token ) token = stky_string_new_charP(Y, 0, 0);
  if ( c == -2 ) c = fgetc(in);
  fprintf(stderr, "  c = %s, state = %d, token = '%s'\n", char_to_str(c), state, token->p);
  switch ( state ) {
  case s_error:
    stky_string_append_char(Y, token, c);
    value = token;
  case s_eos:
  case s_stop:
    break;
  case s_start:
    switch ( c ) {
    case EOF: last_state_2 = s_eos; next_s(s_eos);
    case ' ': case '\t': case '\n': case '\r':
      next_c();
    case '#': c = -2; next_s(s_comment_eol);
    case '/':
      word_lit = 1; c = -2; next_s(s_word);
    case 'a' ... 'z': case 'A' ... 'Z': case '_':
    case '%':
      next_s(s_word);
    case '-': case '+':
      stky_string_append_char(Y, token, c);
      pot_num_c = c; c = -2;
      next_s(s_pot_num);
    case '0' ... '9':
      next_s(s_num);
    case '\'':
      state = s_char; next_c();
    case '"':
      state = s_str; next_c();
    case '{': case '}': case '[': case ']':
      last_state_2 = s_stk_op;
      value = stky_v_char(c); c = -2; next_s(s_stop);
    default:
      next_s(s_error);
    }
  case s_comment_eol:
    switch ( c ) {
    case '\n':
      c = -2; next_s(s_start);
    case EOF:
      next_s(s_start);
    default:
      next_c();
    }
  case s_word:
    switch ( c ) {
    case 'a' ... 'z': case 'A' ... 'Z': case '_': case '0' ... '9':
    case '%':
      stky_string_append_char(Y, token, c);
      next_c();
    case EOF:
    case ' ': case '\t': case '\n': case '\r':
      stky_push(Y, token);
      stky_exec(Y, isn_sym);
      value = stky_pop(Y); next_s(s_stop);
    default:
      abort();
    }
  case s_pot_num:
    switch ( c ) {
    case '0' ... '9':
      next_s(s_num);
    case EOF:
      next_s(s_num);
    case ' ': case '\t': case '\n': case '\r':
      value = token; next_s(s_stop);
    default:
      next_s(s_word);
    }
  case s_num:
    switch ( c ) {
    case '0' ... '9':
      n = n * 10 + c - '0';
      next_c();
    default:
      if ( pot_num_c == '-' ) n = - n;
      value = stky_v_int(n); next_s(s_stop);
    }
  case s_char:
    switch ( c ) {
    default:
      value = stky_v_char(c); next_s(s_stop);
    }
  case s_str:
    switch ( c ) {
    case '"':
      value = token; c = -2; next_s(s_stop);
    default:
      next_c();
    }
  default:
    abort();
  }

 stop:
  if ( c >= 0 ) ungetc(c, in);
  if ( word_lit ) last_state = s_word_lit;
  fprintf(stderr, "  : %p %p\n", (void*) value, (void*) last_state);
  stky_push(Y, value);
  stky_push(Y, stky_v_int(last_state));
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
    case s_word:
      stky_exec(Y, isn_lookup, isn_eval);
      break;
    case s_word_lit: case s_num: case s_char: case s_str:
      break;
    case s_stk_op:
      switch ( c = stky_v_int_(stky_pop(Y)) ) {
      case '[': case '{':
        stky_push(Y, stky_v_mark); break;
      case ']':
        stky_exec(Y, isn_ctm, isn_array_stk, isn_swap, isn_pop); break;
      case '}':
        stky_exec(Y, isn_ctm, isn_array_stk, isn_swap, isn_pop, isn_cve); break;
      default:
        abort();
      }
    }
  }
  return Y;
}

stacky *stky_write(stacky *Y, stky_v v, FILE *out)
{
  int is_dict;
 again:
  is_dict = 0;
  switch ( stky_v_type_i(v) ) {
  case stky_t_null:
    fprintf(out, "@x0 $null"); break;
  case stky_t_int:
    fprintf(out, "%lld", (long long) stky_v_int_(v)); break;
  case stky_t_char:
    fprintf(out, "%s", char_to_str(stky_v_char_(v))); break;
  case stky_t_ref:
    fprintf(out, "@%p $ref", v); break;
  case stky_t_type:
    fprintf(out, "$%s", stky_v_type(v)->name); break;
  case stky_t_string:
    fprintf(out, "\"%s\"", ((stacky_string*) v)->p); break;
  case stky_t_dict:
    is_dict = 1;
  case stky_t_array:
    fprintf(out, "[ ");
    for ( size_t i = 0; i < ((stacky_array*) v)->l; ++ i ) {
      stky_write(Y, ((stacky_array*) v)->p[i], out);
      fprintf(out, " ");
    }
    fprintf(out, "] ");
    if ( is_dict ) {
      stky_write(Y, ((stacky_dict*) v)->eq, out);
      fprintf(out, " dict ");
    }
    break;
  case stky_t_symbol:
    fwrite(((stacky_symbol*) v)->name->p, ((stacky_symbol*) v)->name->l, 1, out);
    break;
  case stky_t_literal:
    fprintf(out, "/"); v = ((stacky_literal*) v)->value; goto again;
  default:
    fprintf(out, "@%p $%s", v, stky_v_type(v)->name); break;
    abort();
  }
  return Y;
}

stacky *stacky_new()
{
  stacky *Y = 0;

  GC_init();
  Y = stky_object_new(Y, stky_t(stacky), sizeof(*Y));
  Y->o.type = stky_t(stacky);
  Y->v_stdin  = stdin;
  Y->v_stdout = stdout;
  Y->v_stderr = stderr;
  Y->v_mark = (void*) stky_string_new_charP(Y, "[", 1);
  ((stacky_object *) Y->v_mark)->type = stky_t(mark);

  {
    stacky_type *t;
#define TYPE(NAME,IND)                                          \
    t = &Y->types[IND];                                         \
    stky_object_init(Y, t, stky_t(type), sizeof(*t));           \
    t->i = IND;                                                 \
    t->name = #NAME;
#include "stacky/types.h"
  }

  stky_object_init(Y, &Y->vs, stky_t(array), sizeof(Y->vs));
  stacky_array_init(Y, &Y->vs, 1024);
  *(Y->vs.p) = Y;

  stky_object_init(Y, &Y->es, stky_t(array), sizeof(Y->es));
  stacky_array_init(Y, &Y->es, 1024);
  *(Y->es.p) = Y;

  {
    char *v; int i;
    if ( (v = getenv("STACKY_TRACE")) && (i = atoi(v)) > 0 )
      Y->trace = i;
  }
  {
    static stky_i e_eq_string[] = { isn_string_eq, isn_rtn, isn_END };
    Y->sym_dict = stky_pop(stky_exec(Y, isn_lit_voidP, (stky_i) e_eq_string, isn_dict_new));
  }
  Y->dict_stack = stky_pop(stky_exec(Y, isn_lit_int, 0, isn_array_new));
  {
    static stky_i e_eq[] = { isn_hdr, isn_eq, isn_rtn, isn_END };
    stky_exec(Y,
              isn_dict_stack,
              isn_lit_voidP, (stky_i) e_eq, isn_dict_new,
              isn_array_push, isn_pop);
  }
  {
    int i;
    for ( i = 0; isn_table[i].name; ++ i ) {
      char name[32] = { 0 };
      stky_v isn_w = stky_words(Y, isn_table[i].isn);
      stky_exec(Y,
                isn_dict_stack_top,
                isn_sym_charP, (stky_i) isn_table[i].isn_sym + 1,
                isn_lit, (stky_i) isn_w,
                isn_dict_set, isn_pop);
      stky_exec(Y,
                isn_dict_stack_top,
                isn_sym_charP, (stky_i) isn_table[i].isn_sym,
                isn_lit, (stky_i) isn_table[i].isn,
                isn_dict_set, isn_pop);
      // -- Y->trace;
    }
  }

  return Y;
}

