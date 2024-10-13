#include <stdio.h>

typedef unsigned long long ulong;
_Static_assert (sizeof(ulong) == 8, "ulong isn't a 8byte word");

typedef struct obj {
  ulong a;
  ulong b;
} obj __attribute__ ((aligned (16)));

#define rnd(a) (((ulong) ((a)+0xf)) & (~0xf))
#define A(o) ((o.a) & ~(0xf)
#define B(o) (o.b)
#define TAG(o) (o.a & 0xf)

#define MEMSIZE 0x100000
ulong M[MEMSIZE];
#define MIDPOINT (MEMSIZE/2)

#define DSTART 0
#define SSTART (MIDPOINT - 1)
#define HSTART (MIDPOINT)
#define HEAPSIZE (MEMSIZE/2)
#define STACKSIZE (MEMSIZE/2)

typedef void (*stack_func)(ulong* env);

ulong SP, HP, DP;

#define CONS_TAG 0
#define SYM_TAG  1
#define INT_TAG  2
#define PROC_TAG 3
#define PRIM_TAG 4

void print_err(char* msg) {
  fputs(msg, stderr);
}
void panic(char* msg) {
  print_err(msg);
  while (1) {}
}

void mk_sym(char* cstr, obj* dest) {
  dest->a = SYM_TAG;
  dest->b = (ulong)cstr;
}
void mk_int(ulong v, obj* dest) {
  dest->a = INT_TAG;
  dest->b = v;
}
void mk_cons(ulong a, ulong b, obj* dest) {
  dest->a = a;
  dest->b = b;
}
void mk_proc(ulong env, ulong body, obj* dest) {
  dest->a = env | PROC_TAG;
  dest->b = body;
}

ulong new_cons(ulong a, ulong b) {
  if (HP - HSTART >= HEAPSIZE) panic("OOM!");
  M[HP++] = a;
  M[HP++] = b;
  return HP-2;
}

char next_char = ' ';
char read_char() {
  char hold = next_char;
  next_char = getchar();
  return hold;
}

char streq(ulong* len, char* a, char* b) {
  ulong count = 0;
  while (*a && *b && *a++ == *b++) ++count;
  *len=count+1;
  return (*a == *b && count != 0) ? 1 : 0;
}

ulong read_token() {
  char* td = (char*) (M+DP);
  char c;
  for (
       *(td++) = c = read_char();
       (next_char != ' ' &&
        next_char != '\n' &&
        next_char != ')') &&
         !(c == '\'' ||
           c == '^' ||
           c == '$' ||
           c == ':' ||
           c == '(' ||
           c == ')');
       *(td++) = c = read_char()
       ) {}
  *(td++) = 0;
  char* indict = M+DSTART;
  char* newsym = (char*)(M+DP);
  ulong len;
  while (!streq(&len, indict, newsym)) {
    indict += len;
    indict = (char*) rnd(indict);
  }
  if (indict == newsym) {// only match is the new sym
    td = (char*) rnd(td);
    DP = (td - (char*)M) / sizeof(ulong);
    return (newsym - (char*)M) / sizeof(ulong);
  } else {                      // exiting match
    return (indict - (char*)M) / sizeof(ulong);
  }
}

void slurp_whitespace() {
  while (next_char == ' ' ||
         next_char == '\t' ||
         next_char == '\n') read_char();
}

struct as_int_t {char b; ulong v;};
struct as_int_t as_int(char* str) {
  ulong val = 0;
  char neg = (*str == '-') ? ++str, -1 : 1;
  for (char c = *str; c != 0; c = *(++str)) {
    if (c < '0' || c > '9') return ((struct as_int_t) {0, 0});
    val = (val*10) + (c - '0');
  }
  val *= neg;
  char* td = (char*)(M+DP);
  while (*(--td) == 0) {}
  while (*(--td) != 0) {}
  DP = ((td+1) - (char*)M)/sizeof(ulong); // remove last dict entry, it was a number
  return ((struct as_int_t) {1, val});
}

// syntax special forms
ulong SQUOTE_SYM;
ulong SPUSH_SYM;
ulong SPOP_SET_SYM;
ulong SPOP_EXT_SYM;

ulong QUOTE_SYM;
ulong PUSH_SYM;
ulong POP_SET_SYM;
ulong POP_EXT_SYM;
ulong OPAREN_SYM;
ulong CPAREN_SYM;
ulong T_SYM;

void p_push(ulong*);
void p_pops(ulong*);
void p_pope(ulong*);
ulong in_list = 0;
ulong read_stack = -1;
ulong read() {
  if (read_stack != -1) {
    ulong hold = M[read_stack];
    read_stack = M[read_stack+1];
    return hold;
  }
  slurp_whitespace();
  ulong raw_sym = read_token();
  ulong ret;
  if (raw_sym == SQUOTE_SYM || raw_sym == QUOTE_SYM) {
    ulong hold = in_list;
    in_list = 0;
    read_stack = new_cons(read(), read_stack);
    ret = new_cons(SYM_TAG, QUOTE_SYM);
    in_list = hold;
  } else if (raw_sym == SPUSH_SYM) {
    ulong hold = in_list;
    in_list = 0;
    read_stack = new_cons(read(),
                          new_cons(new_cons(SYM_TAG, PUSH_SYM),
                                   read_stack));
    ret = new_cons(SYM_TAG, QUOTE_SYM);
    in_list = hold;
  } else if (raw_sym == SPOP_SET_SYM) {
    ulong hold = in_list;
    in_list = 0;
    read_stack = new_cons(read(),
                          new_cons(new_cons(SYM_TAG, POP_SET_SYM),
                                   read_stack));
    ret = new_cons(SYM_TAG, QUOTE_SYM);
    in_list = hold;
  } else if (raw_sym == SPOP_EXT_SYM) {
    ulong hold = in_list;
    in_list = 0;
    read_stack = new_cons(read(),
                          new_cons(new_cons(SYM_TAG, POP_EXT_SYM),
                                   read_stack));
    ret = new_cons(SYM_TAG, QUOTE_SYM);
    in_list = hold;
  } else if (raw_sym == OPAREN_SYM) {
    ulong hold = ++in_list;
    in_list = 0;
    ulong car = read();
    if (car == -1) ret = -1, --in_list;
    in_list = hold;
    ret = car;
  } else if (raw_sym == CPAREN_SYM) {
    if (in_list-- == 0) panic("Unmatched close paren!");
    ret = -1;
  } else {
    char* cstr = (char*)(M + raw_sym);
    struct as_int_t maybe_int = as_int(cstr);
    if (maybe_int.b) ret = new_cons(INT_TAG, maybe_int.v);
    else ret = new_cons(SYM_TAG, raw_sym);
  }
  return (in_list) ? new_cons(ret, read()) : ret;
}

void print(ulong);
void print_list(ulong l) {
  while ((M[l] == CONS_TAG) && (M[l+1] != -1)) {
    print(M[l] & ~0xf);
    l = M[l+1];
  }
}
#define pow(a,b) {typeof(a) _val=1; typeof(b) _b = b; while(_b--) _val*=a; _val;}
void print_int(ulong val) {
  if ((long long) val < 0) putchar('-');
  val &= ~(1ul << 63);
  for (int s = 19; s >= 0; --s) {
    int c = (val / (pow(10, s))) % 10;
    putchar((char)(c + '0'));
  }
}
void print(ulong v) {
  switch (M[v] & 0xf) {
  case CONS_TAG:
    if (M[v+1] == -1) puts("nil");
    else if ((M[M[v+1]] & 0xf) != CONS_TAG) {
      putchar('(');
      print(M[v]);
      puts(" . ");
      print(M[v+1]);
      putchar(')');
    } else {
      putchar('(');
      print_list(v+1);
      putchar(')');
    }
    break;
  case SYM_TAG:
    puts((char*)(M+M[v+1]));
    break;
  case INT_TAG:
    print_int(M[v+1]);
    break;
  case PROC_TAG:
    putchar('[');
    print_list(M[v+1]);
    putchar(']');
    break;
  case PRIM_TAG:
    puts("PRIM");
    break;
  default:
    panic("Unknown tag in print!");
  }
  putchar('\n');
}

ulong lookup(ulong env, ulong raw_sym) {
  do {
    ulong pair = M[env];
    ulong cursym = M[pair];
    ulong _len;
    if (streq(&_len, (char*)(M+raw_sym), (char*)(M+M[cursym+1]))) {
      return M[pair+1];
    }
  } while ((env = M[env+1]) != -1);
  print_err((char*)(M+raw_sym));
  panic(": undefined symbol (lookup)!");
}
void set(ulong env, ulong sym, obj val) {
  do {
    ulong pair = M[env];
    ulong cursym = M[pair];
    ulong _len;
    if (streq(&_len, (char*)(M[sym+1]), (char*)(M[cursym+1]))) {
      M[pair+1] = new_cons(val.a, val.b);
      return;
    }
  } while ((env = M[env+1]) != -1);
  print_err((char*)(M+sym+1));
  panic(": undefined symbol (set)!");
}
ulong extend(ulong env, ulong sym, obj val) {
  return new_cons(new_cons(sym, new_cons(val.a, val.b)),
                  env);
}

void eval(ulong*,ulong);
void compute(ulong* env, ulong body) {
  while (body != -1) {
    ulong cmd = M[body];
    body = M[body+1];
    if (((M[cmd] & 0xf) == SYM_TAG) &&
        M[cmd+1] == QUOTE_SYM) {
      if (body == -1) panic("No data after quote");
      ulong sym = M[body];
      M[--SP] = M[sym+1];
      M[--SP] = SYM_TAG;
      body = M[body+1];
    } else {
      eval(env, cmd);
    }
  }
}

void eval(ulong* env, ulong cur) {
  ulong l;
  switch (M[cur] & 0xf) {
  case CONS_TAG:
    M[--SP] = *env;
    M[--SP] = cur | PROC_TAG;
    break;
  case SYM_TAG:
    // We catch this in both eval and in compute. So we know if we
    // catch it here it has to be a top-level
    if (M[cur+1] == QUOTE_SYM) {
      ulong sym = read();
      M[--SP] = M[sym+1];
      M[--SP] = M[sym];
    } else if ((l = lookup(*env, M[cur+1])) == -1) {
      M[--SP] = -1;
      M[--SP] = CONS_TAG;
    } else {
      eval(env, l);
    }
    break;
  case INT_TAG:
    M[--SP] = M[cur+1];
    M[--SP] = M[cur];
    break;
  case PROC_TAG:
    {
      ulong capenv = M[cur] & ~0xf;
      ulong body = M[cur+1];
      compute(&capenv, body);
      break;
    }
  case PRIM_TAG:
    ((stack_func)(M[cur+1]))(env);
    break;
  default:
    panic("Unknown tag in eval!");
  }
}

void p_push(ulong* env) {
  if (M[SP] != SYM_TAG) panic("push on non-sym!");
  ulong h = lookup(*env, M[SP+1]);
  M[SP+1] = M[h+1];
  M[SP] = M[h];
}
void p_pope(ulong* env) {
  if (M[SP] != SYM_TAG) panic("pope on non-sym!");
  ulong s = new_cons(M[SP], M[SP+1]);
  obj tmp = {M[SP+2], M[SP+3]};
  SP+=4;
  *env = extend(*env, s, tmp);
}
void p_pops(ulong* env) {
  if (M[SP] != SYM_TAG) panic("pops on non-sym!");
  ulong s = new_cons(M[SP], M[SP+1]);
  obj tmp = {M[SP+2], M[SP+3]};
  SP+=4;
  set(*env, s, tmp);
}

void p_eq(ulong* env) {
  char r = (M[SP] == M[SP+2]) && (M[SP+1] == M[SP+3]);
  SP+=2;
  if (r) {
    M[SP] = SYM_TAG;
    M[SP+1] = T_SYM;
  } else {
    M[SP] = CONS_TAG;
    M[SP+1] = -1;
  }
}
void p_cons(ulong* env) {
  ulong car = new_cons(M[SP], M[SP+1]);
  ulong cdr = new_cons(M[SP+2], M[SP+3]);
  SP+=2;
  M[SP] = car | CONS_TAG;
  M[SP+1] = cdr;
}
void p_car(ulong* env) {
  if ((M[SP] & 0xf) != CONS_TAG) panic("Non-cons in car!");
  ulong h = M[SP] & 0xf;
  M[SP] = M[h];
  M[SP+1] = M[h+1];
}
void p_cdr(ulong* env) {
  if ((M[SP] & 0xf) != CONS_TAG) panic("Non-cons in car!");
  ulong h = M[SP+1];
  M[SP] = M[h];
  M[SP+1] = M[h+1];
}
void p_cswap(ulong* env) {
  ulong b = (M[SP++] == SYM_TAG) && (M[SP++] == T_SYM);
  if (b) {
    ulong a[4] = {M[SP], M[SP+1], M[SP+2], M[SP+3]};
    M[SP] = a[2];
    M[SP+1] = a[3];
    M[SP+2] = a[0];
    M[SP+3] = a[1];
  }
}
void p_tag(ulong* env) {
  M[SP+1] = M[SP] & 0xf;
  M[SP] = INT_TAG;
}
void p_read(ulong* env) {
  ulong h = read();
  if (h == -1) {
    M[--SP] = -1;
    M[--SP] = CONS_TAG;
  } else {
    M[--SP] = M[h+1];
    M[--SP] = M[h];
  }
}
void p_print(ulong* env) {
  print(SP);
  SP+=2;
}

void p_sstack(ulong* env) {
  for (ulong a = SSTART-1; a >= SP; a-=2) {
    print(a);
  }
}
void p_env(ulong* env) {
  M[--SP] = M[(*env) + 1];
  M[--SP] = M[*env];
}
void p_dup(ulong* env) {
  SP-=2;
  M[SP] = M[SP+2];
  M[SP+1] = M[SP+3];
}
void p_drop(ulong* env) {
  SP+=2;
}
void p_add(ulong* env) {
  if (((M[SP] & 0xf) != INT_TAG) || ((M[SP+2] & 0xf) != INT_TAG)) panic("Adding non-ints!");
  ulong v = M[SP+1];
  SP+=2;
  M[SP+1] += v;
}
void p_sub(ulong* env) {
  if (((M[SP] & 0xf) != INT_TAG) || ((M[SP+2] & 0xf) != INT_TAG)) panic("Subtracting non-ints!");
  ulong v = M[SP+1];
  SP+=2;
  M[SP+1] -= v;
}
void p_mul(ulong* env) {
  if (((M[SP] & 0xf) != INT_TAG) || ((M[SP+2] & 0xf) != INT_TAG)) panic("Multiplying non-ints!");
  ulong v = M[SP+1];
  SP+=2;
  M[SP+1] *= v;
}
void p_div(ulong* env) {
  if (((M[SP] & 0xf) != INT_TAG) || ((M[SP+2] & 0xf) != INT_TAG)) panic("Dividing non-ints!");
  ulong v = M[SP+1];
  SP+=2;
  M[SP+1] /= v;
}
void p_mod(ulong* env) {
  if (((M[SP] & 0xf) != INT_TAG) || ((M[SP+2] & 0xf) != INT_TAG)) panic("Modulo non-ints!");
  ulong v = M[SP+1];
  SP+=2;
  M[SP+1] %= v;
}
void p_lsh(ulong* env) {
  if (((M[SP] & 0xf) != INT_TAG) || ((M[SP+2] & 0xf) != INT_TAG)) panic("Left shift non-ints!");
  ulong v = M[SP+1];
  SP+=2;
  M[SP+1] <<= v;
}
void p_rsh(ulong* env) {
  if (((M[SP] & 0xf) != INT_TAG) || ((M[SP+2] & 0xf) != INT_TAG)) panic("Right shift non-ints!");
  ulong v = M[SP+1];
  SP+=2;
  M[SP+1] >>= v;
}
void p_nand(ulong* env) {
  if (((M[SP] & 0xf) != INT_TAG) || ((M[SP+2] & 0xf) != INT_TAG)) panic("Nand non-ints!");
  ulong v = M[SP+1];
  SP+=2;
  M[SP+1] = ~(M[SP+1] & v);
}

void p_load(ulong* env) {
  if ((M[SP] & 0xf) != INT_TAG) panic("Non-int in load");
  M[SP+1] = *((ulong*) M[SP+1]);
}
void p_store(ulong* env) {
  if ((M[SP] & 0xf) != INT_TAG || (M[SP+2] & 0xf) != INT_TAG) panic("Non-int in store");
  ulong* addr = M[SP+1];
  SP+=2;
  M[SP+1] = *addr;
}
void p_obj_to_ptr(ulong* env) {
  M[SP+1] = new_cons(M[SP], M[SP+1]);
  M[SP] = INT_TAG;
}
void p_ptr_to_obj(ulong* env) {
  if ((M[SP] & 0xf) != INT_TAG) panic("Non-int in ptr_to_obj");
  M[SP] = *((ulong*) M[SP+1]);
  M[SP+1] = *((ulong*) M[SP+1]+1);
}

ulong env_define_prim(ulong env, ulong raw_sym, stack_func prim) {
  return new_cons(new_cons(new_cons(SYM_TAG, raw_sym),
                           new_cons(PRIM_TAG, prim)),
                  env);
}

void strcpy_inc(char** dest, char* src) {
  while (*((*dest)++) = *src++) {}
}

#define BAKE_DEF(cstr, prim)                                            \
  {                                                                     \
    dict = rnd(dict);                                                   \
    env = env_define_prim(env, (ulong)(dict - (char*)M) / sizeof(ulong), prim); \
    strcpy_inc(&dict, cstr);                                            \
  }

int main() {
  SP = SSTART;
  HP = HSTART;

  char* dict = (char*)M;
  ulong env = -1;

  // strings for special syntax forms
  dict = rnd(dict);
  QUOTE_SYM = (dict - (char*)M) / sizeof(ulong);
  strcpy_inc(&dict, "'");

  dict = rnd(dict);
  SPUSH_SYM = (dict - (char*)M) / sizeof(ulong);
  strcpy_inc(&dict, "$");

  dict = rnd(dict);
  SPOP_SET_SYM = (dict - (char*)M) / sizeof(ulong);
  strcpy_inc(&dict, "^");

  dict = rnd(dict);
  SPOP_EXT_SYM = (dict - (char*)M) / sizeof(ulong);
  strcpy_inc(&dict, ":");

  dict = rnd(dict);
  OPAREN_SYM = (dict - (char*)M) / sizeof(ulong);
  strcpy_inc(&dict, "(");

  dict = rnd(dict);
  CPAREN_SYM = (dict - (char*)M) / sizeof(ulong);
  strcpy_inc(&dict, ")");

  dict = rnd(dict);
  T_SYM = (dict - (char*)M) / sizeof(ulong);
  strcpy_inc(&dict, "t");

  dict = rnd(dict);
  QUOTE_SYM = (dict - (char*)M) / sizeof(ulong);
  strcpy_inc(&dict, "quote");

  // primitives
  BAKE_DEF("push", p_push);
  PUSH_SYM = (dict - (char*)M) / sizeof(ulong);
  BAKE_DEF("pops", p_pops);
  POP_SET_SYM = (dict - (char*)M) / sizeof(ulong);
  BAKE_DEF("pope", p_pope);
  POP_EXT_SYM = (dict - (char*)M) / sizeof(ulong);

  BAKE_DEF("eq", p_eq);
  BAKE_DEF("cons", p_cons);
  BAKE_DEF("car", p_car);
  BAKE_DEF("cdr", p_cdr);
  BAKE_DEF("cswap", p_cswap);
  BAKE_DEF("tag", p_tag);
  BAKE_DEF("read", p_read);
  BAKE_DEF("print", p_print);

  BAKE_DEF("sstack", p_sstack);
  BAKE_DEF("env", p_env);
  BAKE_DEF("dup", p_dup);
  BAKE_DEF("drop", p_drop);
  BAKE_DEF("add", p_add);
  BAKE_DEF("sub", p_sub);
  BAKE_DEF("mul", p_mul);
  BAKE_DEF("div", p_div);
  BAKE_DEF("mod", p_mod);
  BAKE_DEF("lsh", p_lsh);
  BAKE_DEF("rsh", p_rsh);
  BAKE_DEF("nand", p_nand);

  BAKE_DEF("load", p_load);
  BAKE_DEF("store", p_store);
  BAKE_DEF("obj_to_ptr", p_obj_to_ptr);
  BAKE_DEF("ptr_to_obj", p_ptr_to_obj);

  DP = (ulong*)rnd(dict) - M;

  read_char();                  // clear the dummy peek char

  while (1) {
    eval(&env, read());
  }
}






