/* This is strongly inspired by sectorlisp:
   https://github.com/jart/sectorlisp */

// these are offsets into S
#define kT          4
#define kQuote      6
#define kCond       12
#define kRead       17
#define kPrint      22
#define kAtom       28
#define kCar        33
#define kCdr        37
#define kCons       41
#define kEq         46
#define kGetc       49
#define kPutc       54

#define M (RAM + sizeof(RAM) / sizeof(RAM[0]) / 2)
#define S "NIL\0T\0QUOTE\0COND\0READ\0PRINT\0ATOM\0CAR\0CDR\0CONS\0EQ\0GETC\0PUTC"

// if sign bit is set, it's a pointer to a cell, if not it's an atom
typedef int cell;

cell cx; /* points the the lowest used cell addr. alloc with --c */
cell dx; /* stores lookahead character */
cell RAM[0100000];

extern char inner_getchar(void);
extern void inner_putchar(char);

void setup() {
  dx = inner_getchar();
  cx = 0;
  for (int i = 0; i < sizeof(S); ++i) M[i] = S[i]; // inital wordlist
}

char lisp_getchar() {
  char hold = dx;
  dx = inner_getchar();
  return hold;
}

cell get_token(void) {
  int c, i = 0;
  do if ((c = lisp_getchar()) > ' ') RAM[i++] = c;
  // ^ copy into new string
  while (c <= ' ' || ((c > ')') && dx > ')'));
  // ^ while we haven't closed a paren or it's whitespace
  RAM[i] = 0;                   // null terminator
  return c;
}

// lisp forward decs -------------------------------------------------
cell car(cell);
cell cdr(cell);
cell cons(cell, cell);

// helper funcs ------------------------------------------------------

// return an offset in M to a cstr matching the word in RAM, copying
// into M if necessary. The current token via get_token is in the
// bottom of RAM.
cell intern() {
  int i, j, x;
  for (i = 0; (x = M[i++]);) {  // iterate over the chars of S (in M)
    for (j = 0;; ++j) {
      if (x != RAM[j]) break;   // if they aren't equal, skip to next
      if (!x) return i - j - 1; // if eq to null, then return offset
      x = M[i++];
    }
    while (x) x = M[i++];
    // ^ slurp until null terminator (skip to next)
  }
  j = 0;
  // ^ RAM+0 points to str that is not already in M's dict
  x = --i;
  while (M[i++] = RAM[j++]);
  // ^ add to the list of words at the start of M
  return x;
  // ^ return the location of the head of the new word
}

// mutual recursion, we need forward decs
cell add_list(cell);
cell get_obj(int);

// interprete the input as a list and recursively build it up
cell get_list() {
  cell c = get_token();
  return c == ')' ? 0 : add_list(get_obj(c));
}

// prepend to a list
cell add_list(cell h) {
  return cons(h, get_list());
}

cell get_obj(cell c) {
  return c == '(' ? get_list() : intern();
}

// read in some balanced s-expr
cell read() {
  return get_obj(get_token());
}

// print an atom (c-str) starting at x
void print_atom(cell x) {
  int c;
  while (c = M[x++]) {
    inner_putchar(c);
  }
}

// mutually recursively print out an s-expr
void print_obj(cell);           // forward dec
void print_list(cell x) {
  inner_putchar('(');
  print_obj(car(x));
  while (x = cdr(x)) {
    if (x < 0) {
      inner_putchar(' ');
      print_obj(car(x));
    } else {
      inner_putchar('.');
      print_obj(x);
      break;
    }
  }
  inner_putchar(')');
}

void print_obj(cell x) {
  if (x < 0) {
    print_list(x);
  } else {
    print_atom(x);
  }
}

void print(cell c) { print_obj(c); }

extern void print_ln(void);

// lisp defs ---------------------------------------------------------

cell car(cell a) {return M[a];}

cell cdr(cell a) {return M[a+1];}

cell cons(cell car, cell cdr) {
  M[--cx] = cdr;
  M[--cx] = car;
  return cx;
}

// if x is higher (aka older) than m, passthrough
// else copy recursively into lower cells, and return their addr + k
//
// called every eval with x as the new returned val, m as cx before
// the new allocs, and k space used by the new allocs. The tree of
// cons cells returned here will be laid out in a block in memory, and
// will have all their pointers offset by k.
cell gc(cell x, cell m, cell k) {
  return x < m ? cons(gc(car(x), m, k),
                      gc(cdr(x), m, k)) + k : x;
}

cell eval(cell, cell);          // forward dec

// nil -> nil, else `map eval m` with environment a
cell eval_list(cell m, cell a) {
  if (m) {
    cell x = eval(car(m), a);
    return cons(x, eval_list(cdr(m), a));
  } else return 0;
}

// take three lists, zip the first and second and concat the zip with
// a. Assumes len(y) >= len(x)
cell pair_list(cell x, cell y, cell a) {
  return x ? cons(cons(car(x), car(y)),
                  pair_list(cdr(x), cdr(y), a)) : a;
}

// return the obj matching x in the association list y, nil otherwise
cell assoc(cell x, cell y) {
  if (!y) return 0;
  if (x == car(car(y))) return cdr(car(y));
  return assoc(x, cdr(y));
}

// Evaluate a conditional form. If the head of the head is true,
// evaluate the head of body, else continue to the next
cell eval_cond(cell c, cell a) {
  if (eval(car(car(c)), a)) {
    return eval(car(cdr(car(c))), a);
  } else {
    return eval_cond(cdr(c), a);
  }
}

// function application. apply f to list x of args in env a;
//
// if f is a cell pointer,
// then car(cdr(cdr(f))) is the body, and car(cdr(f)) is the arg_list.
// car(f) can be anything, but is often (QUOTE LAMBDA)
cell apply(cell f, cell x, cell a) {
  if (f < 0) return eval(car(cdr(cdr(f))),
                         pair_list(car(cdr(f)), x, a));
  if (f > kPutc) return apply(eval(f, a), x, a);
  if (f == kEq) return car(x) == car(cdr(x)) ? kT : 0;
  if (f == kCons) return cons(car(x), car(cdr(x)));
  if (f == kAtom) return car(x) < 0 ? 0 : kT;
  if (f == kCar) return car(car(x));
  if (f == kCdr) return cdr(car(x));
  if (f == kRead) return read();
  if (f == kPrint) return (x ? print(car(x)) : print_ln()), 0;
  if (f == kGetc) return lisp_getchar(), dx;
  if (f == kPutc) return inner_putchar((char) car(x)), 0;
}

// select action and do it, cleaning garbage if allocations occured.
cell eval(cell e, cell a) {
  int A, B, C;
  if (e >= 0) return assoc(e, a); // lookup symbol
  if (car(e) == kQuote) return car(cdr(e)); // quote
  A = cx;                                   // pre-alloc marker
  if (car(e) == kCond) {        // conditional
    e = eval_cond(cdr(e), a);
  } else {                      // func application
    e = apply(car(e), eval_list(cdr(e), a), a);
  }
  B = cx;                       // post-alloc marker
  e = gc(e, a, A-B);            // e is root of new tree offset by A-B
  C = cx;                       // post-gc marker
  while (C < B) M[--A] = M[--B];
  // ^ copy block of memory with gc'd tree back over itself + garbage,
  // undoing the A-B offset
  cx = A;                       // release unused memory
  return e;
}

// backend should supply main that calls setup, and loops
// (print(eval(read(), 0))), print_ln.
