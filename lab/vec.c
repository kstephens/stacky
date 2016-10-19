#define T_(N) T##_##N
#ifndef T_box
#define T_box(V) (V)
#define T_unbox(V) (V)
#endif

void stky_array_resize(stky_array *self, size_t s)
{
  size_t ms = sizeof(self->b[0]) * (s + 1);
  stky_v *b = self->b ?
    GC_realloc(self->b, ms) :
    GC_malloc(ms);
  self->p = b + (self->p - self->b);
  *(self->e = (self->b = b) + (self->s = s)) = 0;
}
void stky_array_init(stky_array* self, size_t s)
{
  self->b = self->p = self->e = 0;
  self->s = 0;
  stky_array_resize(self, s);
}
void stky_array_push(stky_array *self, stky_v v)
{
  if ( self->p >= self->e )
    stky_array_resize(self, self->s * 3 / 2 + 3);
  *(self->p ++) = v;
}
stky_v stky_array_pop(stky_array *self)
{
  if ( self->p <= self->b )
    abort();
  return *(-- self->p);
}

stky_F(array_make) // v1 v2 ... vn n | [ v1 v2 ... vn ]
{
  size_t s = stky_v_i_(stky_pop());
  stky_array *a = stky_type_alloc(t_array);
  stky_array_init(a, s);
  for ( size_t i = 0; i < s; ++ i )
    a->b[i] = V(s - 1 - i);
  a->p = a->b + s;
  stky_popn(s);
  stky_push(stky_v_o(a));
}

stky_F(array_push) { // v array |
  stky_array* a = stky_O(stky_pop(), array);
  stky_v v = stky_pop();
  stky_array_push(a, v);
}
stky_F(array_len) { // [ e0 ... en ] | n - 1
  stky_array* a = stky_O(stky_pop(), array);
  stky_push(stky_v_i(a->p - a->b));
}
stky_F(array_top) { // [ e0 ... en ] | en
  V(0) = stky_O(V(0), array)->p[-1];
}
stky_F(array_get) { // [ e0 ... ] i | ei
  stky_i i = stky_v_i_(stky_pop());
  stky_array* a = stky_O(stky_pop(), array);
  stky_push(a->b[i]);
}
stky_F(array_each) { // [ e0 .. en ] f | e0 f .. en f
  stky_v f = stky_pop();
  stky_array* a = stky_O(stky_pop(), array);
  stky_v *p = a->b;
  if ( p < a->p ) {
    while ( p < a->p - 1 ) {
      stky_push(*(p ++));
      stky_eval(f);
    }
    stky_push(*p);
    stky_eval(f); // TAILCALL
  }
}

#undef T
#undef T_
#undef T_box
#undef T_unbox
