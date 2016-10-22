#ifndef T__
#define T__(T,N) T##_##N
#define T_(T,N) T_(T,N)
#define stky_T_(T,N)stky_##T##N
#define stky_T(T,N)stky_T_(T,N)
#define stky_FT_(T,N)stky_F(T##_##N)
#define stky_FT(T,N)stky_FT_(T,N)
#endif

void stky_T(T,_resize)(stky_T(T,)* self, size_t s)
{
  size_t ms = sizeof(self->b[0]) * (s + 1);
  T_t* b = self->b ?
    GC_realloc(self->b, ms) :
    GC_malloc(ms);
  self->p = b + (self->p - self->b);
  *(self->e = (self->b = b) + (self->s = s)) = 0;
}
void stky_T(T,_init)(stky_T(T,)* self, size_t s)
{
  self->b = self->p = self->e = 0;
  self->s = 0;
  stky_T(T,_resize)(self, s);
}
void stky_T(T,_push)(stky_T(T,)* self, stky_v v)
{
  if ( self->p >= self->e )
    stky_T(T,_resize)(self, self->s * 3 / 2 + 3);
  *(self->p ++) = T_unbox(v);
}
stky_v stky_T(T,_pop)(stky_T(T,)* self)
{
  return self->p > self->b ? T_box(*(-- self->p)) : 0;
}

stky_FT(T,make) // v1 v2 ... vn n | [ v1 v2 ... vn ]
{
  size_t s = stky_v_i_(stky_pop());
  stky_T(T,)* self = stky_type_alloc(paste2(t_,T));
  stky_T(T,_init)(self, s);
  for ( size_t i = 0; i < s; ++ i )
    self->b[i] = T_unbox(V(s - 1 - i));
  self->p = self->b + s;
  stky_popn(s);
  stky_push(stky_v_o(self));
}
stky_FT(T,push) { // v array |
  stky_T(T,)* self = stky_O(stky_pop(), T);
  stky_v v = stky_pop();
  stky_T(T,_push)(self, v);
}
stky_FT(T,len) { // [ e0 ... en ] | n - 1
  stky_T(T,)* self = stky_O(stky_pop(), T);
  stky_push(stky_v_i(self->p - self->b));
}
stky_FT(T,top) { // [ e0 ... en ] | en
  V(0) = T_box(stky_O(V(0), T)->p[-1]);
}
stky_FT(T,get) { // i [ e0 ... ] | ei
  stky_T(T,)* self = stky_O(stky_pop(), T);
  stky_i i = stky_v_i_(stky_pop());
  stky_push(T_box(self->b[i]));
}
stky_FT(T,set) { // v i [ e0 ... ] |
  stky_T(T,)* self = stky_O(stky_pop(), T);
  stky_i i = stky_v_i_(stky_pop());
  stky_v v = stky_pop();
  self->b[i] = T_unbox(v);
}
stky_FT(T,each) { // [ e0 .. en ] f | e0 f .. en f
  stky_v f = stky_pop();
  stky_T(T,)* self = stky_O(stky_pop(), T);
  T_t* p = self->b;
  if ( p < self->p ) {
    while ( p + 1 < self->p ) {
      stky_push(T_box(*(p ++)));
      stky_eval(f);
    }
    stky_push(T_box(*p));
    stky_eval(f); // TAILCALL
  }
}

#undef T
#undef T_t
#undef T_box
#undef T_unbox
