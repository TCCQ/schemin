#ifndef BAREMETAL
#include <stdio.h>
void putstring(char* s) {fputs(s, stdout);}
#else
extern char getchar(void);
extern void putchar(char);
extern void putstring(char*);
#endif

typedef unsigned long long ulong;
_Static_assert (sizeof(ulong) == 8, "ulong isn't a 8byte word");
_Static_assert (sizeof(ulong*) == 8, "ulong* isn't a 8byte word");

#define RND_UP(a) ((ulong*) (((ulong)((a)+0xf)) & (~0xfULL)))
#define ADDR_MASK(a) ((ulong*) ((ulong) (a) & (~0xfULL)))
#define TAG_MASK(a) (((ulong) (a)) & 0xf)
#define FST(a) (ADDR_MASK(a)[0])
#define SND(a) (ADDR_MASK(a)[1])

#define MEMSIZE 0x8000
#define MIDPOINT (MEMSIZE/2)
#define DSTART 0
#define SSTART (MIDPOINT - 16)
// ^ TODO not strictly necessary
#define HSTART (MIDPOINT)
#define HEAPSIZE (MEMSIZE/2)
#define SEMIHEAPSIZE (HEAPSIZE/2)
#define STACKSIZE (MEMSIZE/2)

#ifdef BAREMETAL
extern char* M;
#else
char M[MEMSIZE];
#endif

typedef void (*stack_func)(ulong** env);
ulong *SP, *env;
char *DP;

#define CONS_TAG   0
#define SYM_TAG    1
#define INT_TAG    2
#define PROC_TAG   3
#define PRIM_TAG   4
#define GC_FWD_TAG 5

#ifdef BAREMETAL
extern void print_err(char*);
#else
void print_err(char* msg) {
  fputs(msg, stderr);
}
#endif
void p_sstack(ulong**);
void panic(char* msg) {
  print_err(msg);
  p_sstack(&env);
  while (1) {}
}

ulong *tospace, *fromspace, *HP;
ulong* copy(ulong* obj) {
  ulong* newaddr;
  if (TAG_MASK(FST(obj)) == GC_FWD_TAG) return SND(obj);
  FST(HP) = FST(obj);
  SND(HP) = SND(obj);
  newaddr = HP;
  HP += 2;
  FST(obj) = GC_FWD_TAG;
  SND(obj) = newaddr;
  return newaddr;
}

void collect(ulong** env) {
  print_err("GC!");
  ulong* scan;
  ulong* hold = fromspace;
  fromspace = tospace;
  tospace = hold;
  HP = fromspace;
  scan = fromspace;

  *env = copy(*env);

  for (ulong** a = (ulong**)(M+SSTART-16); a >= (ulong**)SP; a-=2) {
    *a = copy(*a);
  }

  while (scan < HP) {
    switch (TAG_MASK(FST(scan))) {
    case CONS_TAG:
      FST(scan) = copy(FST(scan));
      SND(scan) = copy(SND(scan));
      break;
    case PROC_TAG:
      FST(scan) = (ulong)copy(ADDR_MASK(FST(scan))) | PROC_TAG;
      SND(scan) = copy(SND(scan));
      break;
    default:
      break;
    }
    scan += 2;
  }
}

ulong* new_cons(ulong a, ulong b) {
  if (HP + 2 > fromspace + SEMIHEAPSIZE) {
    collect(&env);
  }
  if (HP + 2 > fromspace + SEMIHEAPSIZE) {
    panic("OOM!");
  }
  FST(HP) = a;
  SND(HP) = b;
  HP+=2;
  return HP-2;
}

char next_char = ' ';
char read_char() {
  char hold = next_char;
  next_char = getchar();
  return hold;
}

char streq(ulong* len, char* a, char* b) {
  /* Returns non-zero if a and b point to matching strings, or zero
     otherwise. Places the length of a including null terminator in
     location pointed to by len regardless of return status. */
  *len = 0;
  while (1) {
    if (*a != *b) {
      while (*(a++)) ++(*len);
      return 0;
    } else if (!*a && !*b) {
      return 1;
    } else {
      ++a;
      ++b;
      ++(*len);
    }
  }
}

char* read_token() {
  char* td = DP;
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
  char* newsym = DP;
  ulong len;
  while (!streq(&len, indict, newsym)) {
    indict += len;
    while (*indict++) {}
  }
  if (indict == newsym) {// only match is the new sym
    DP = td;
    return newsym;
  } else {                      // exiting match
    return indict;
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
  while (*(--DP) == 0) {}       // remove the last dictionary entry
  while (*(--DP) != 0) {}
  DP++;
  return ((struct as_int_t) {1, val});
}

// syntax special forms
char* SQUOTE_SYM;
char* SPUSH_SYM;
char* SPOP_SET_SYM;
char* SPOP_EXT_SYM;

char* QUOTE_SYM;
char* PUSH_SYM;
char* POP_SET_SYM;
char* POP_EXT_SYM;
char* OPAREN_SYM;
char* CPAREN_SYM;
char* T_SYM;

void p_push(ulong**);
void p_pops(ulong**);
void p_pope(ulong**);
ulong* read_stack = -1;
ulong* read() {
  ulong* ret;
  if (read_stack != -1) {
    ret = FST(read_stack);
    read_stack = SND(read_stack);
  } else {
    slurp_whitespace();
    char* raw_sym = read_token();
    if (raw_sym == SQUOTE_SYM || raw_sym == QUOTE_SYM) {
      read_stack = new_cons(read(), read_stack);
      ret = new_cons(SYM_TAG, QUOTE_SYM);
    } else if (raw_sym == SPUSH_SYM) {
      read_stack = new_cons(read(),
                            new_cons(new_cons(SYM_TAG, PUSH_SYM),
                                     read_stack));
      ret = new_cons(SYM_TAG, QUOTE_SYM);
    } else if (raw_sym == SPOP_SET_SYM) {
      read_stack = new_cons(read(),
                            new_cons(new_cons(SYM_TAG, POP_SET_SYM),
                                     read_stack));
      ret = new_cons(SYM_TAG, QUOTE_SYM);
    } else if (raw_sym == SPOP_EXT_SYM) {
      read_stack = new_cons(read(),
                            new_cons(new_cons(SYM_TAG, POP_EXT_SYM),
                                     read_stack));
      ret = new_cons(SYM_TAG, QUOTE_SYM);
    } else if (raw_sym == OPAREN_SYM) {
      ret = read();
      if (ret == -1) return -1;
      ulong* head = new_cons(ret, -1);
      ulong* tail = head;
      while ((ret = read()) != -1) {
        tail = SND(tail) = new_cons(ret, -1);
      }
      ret = head;
    } else if (raw_sym == CPAREN_SYM) {
      ret = -1;
    } else {
      char* cstr = raw_sym;
      struct as_int_t maybe_int = as_int(cstr);
      if (maybe_int.b) ret = new_cons(INT_TAG, maybe_int.v);
      else ret = new_cons(SYM_TAG, raw_sym);
    }
  }
  return ret;
}

void print(ulong*, char);
void print_list(ulong* l) {
  while (l != -1 && (TAG_MASK(FST(l)) == CONS_TAG)) {
    print(FST(l), 0);
    if (SND(l) != -1) putchar(' ');
    l = SND(l);
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
void print(ulong* v, char newline) {
  switch (TAG_MASK(FST(v))) {
  case CONS_TAG:
    if (ADDR_MASK(FST(v)) == -1 && SND(v) == -1) putstring("nil");
    else if (TAG_MASK(FST(SND(v))) != CONS_TAG) {
      putchar('(');
      print(FST(v), 0);
      putstring(" . ");
      print(SND(v), 0);
      putchar(')');
    } else {
      putchar('(');
      print_list(v);
      putchar(')');
    }
    break;
  case SYM_TAG:
    putstring((char*)SND(v));
    break;
  case INT_TAG:
    print_int((ulong)SND(v));
    break;
  case PROC_TAG:
    putchar('[');
    print_list(SND(v));
    putchar(']');
    break;
  case PRIM_TAG:
    putstring("PRIM");
    break;
  default:
    panic("Unknown tag in print!");
  }
  if (newline) putchar('\n');
}

ulong* lookup(ulong* env, char* raw_sym) {
  do {
    ulong* pair = FST(env);
    ulong* cursym = FST(pair);
    ulong _len;
    if (streq(&_len, (char*)SND(cursym), raw_sym)) {
      return SND(pair);
    }
  } while ((env = SND(env)) != -1);
  print_err((char*)raw_sym);
  panic(": undefined symbol (lookup)!");
}
void set(ulong* env, ulong* sym, ulong* val) {
  // NOTE not safe if val is a stack pointer, needs to be a heap cell
  do {
    ulong* pair = FST(env);
    ulong* cursym = FST(pair);
    ulong _len;
    if (streq(&_len, (char*)SND(sym), (char*)SND(cursym))) {
      SND(pair) = val;
      return;
    }
  } while ((env = SND(env)) != -1);
  print_err((char*)SND(sym));
  panic(": undefined symbol (set)!");
}
ulong extend(ulong* env, ulong* sym, ulong* val) {
  // NOTE not safe if val is a stack pointer, needs to be a heap cell
  return new_cons(new_cons(sym, val), env);
}

void eval(ulong**,ulong*);
void compute(ulong** env, ulong* body) {
  while (body != -1) {
    ulong* cmd = FST(body);
    body = SND(body);
    if ((TAG_MASK(FST(cmd)) == SYM_TAG) &&
        (char*)SND(cmd) == QUOTE_SYM) {
      if (body == -1) panic("No data after quote");
      ulong* sym = FST(body);
      *(--SP) = SND(sym);
      *(--SP) = SYM_TAG;
      body = SND(body);
    } else {
      eval(env, cmd);
    }
  }
}

void eval(ulong** env, ulong* cur) {
  if (SP-2 <= DP) panic("Stack overflow!");
  ulong l;
  if (cur == -1) {
    // push nil
    *(--SP) = -1;
    *(--SP) = (ulong)ADDR_MASK(-1) | CONS_TAG;
  }
  switch (TAG_MASK(FST(cur))) {
  case CONS_TAG:
    *(--SP) = (ulong)cur;
    *(--SP) = (ulong)(*env) | PROC_TAG;
    break;
  case SYM_TAG:
    // We catch this in both eval and in compute. So we know if we
    // catch it here it has to be a top-level
    if (SND(cur) == QUOTE_SYM) {
      ulong* sym = read();
      *(--SP) = SND(sym);
      *(--SP) = FST(sym);
    } else if ((l = lookup(*env, SND(cur))) == -1) {
      *(--SP) = -1;
      *(--SP) = CONS_TAG;
    } else {
      eval(env, l);
    }
    break;
  case INT_TAG:
    *(--SP) = SND(cur);
    *(--SP) = FST(cur);
    break;
  case PROC_TAG:
    {
      ulong* capenv = ADDR_MASK(FST(cur));
      ulong* body = SND(cur);
      compute(&capenv, body);
      FST(cur) = (ulong)capenv | PROC_TAG; // write back changes
      break;
    }
  case PRIM_TAG:
    ((stack_func)(SND(cur)))(env);
    break;
  default:
    panic("Unknown tag in eval!");
  }
}

void p_push(ulong** env) {
  if (*SP != SYM_TAG) panic("push on non-sym!");
  ulong* h = lookup(*env, *(SP+1));
  *(SP+1) = *(h+1);
  *SP = *h;
}
void p_pope(ulong** env) {
  if (*SP != SYM_TAG) panic("pope on non-sym!");
  ulong* s = new_cons(*SP, *(SP+1));
  ulong* tmp = new_cons(*(SP+2), *(SP+3));
  SP+=4;
  *env = extend(*env, s, tmp);
}
void p_pops(ulong** env) {
  if (*SP != SYM_TAG) panic("pops on non-sym!");
  ulong* s = new_cons(*SP, *(SP+1));
  ulong* tmp = new_cons(*(SP+2), *(SP+3));
  SP+=4;
  set(*env, s, tmp);
}

void p_eq(ulong** env) {
  char r = (*SP == *(SP+2)) && (*(SP+1) == *(SP+3));
  SP+=2;
  if (r) {
    *SP = SYM_TAG;
    *(SP+1) = T_SYM;
  } else {
    *SP = (ulong)ADDR_MASK(-1) | CONS_TAG;
    *(SP+1) = -1;
  }
}
void p_cons(ulong** env) {
  ulong* car = new_cons(*SP, *(SP+1));
  ulong* cdr = new_cons(*(SP+2), *(SP+3));
  SP+=2;
  *SP = ((ulong)car) | CONS_TAG;
  *(SP+1) = cdr;
}
void p_car(ulong** env) {
  if (TAG_MASK(*SP) != CONS_TAG) panic("Non-cons in car!");
  ulong* h = ADDR_MASK(*SP);
  *SP = FST(h);
  *(SP+1) = SND(h);
}
void p_cdr(ulong** env) {
  if (TAG_MASK(SP) != CONS_TAG) panic("Non-cons in car!");
  ulong* h = *(SP+1);
  *SP = FST(h);
  *(SP+1) = SND(h);
}
void p_cswap(ulong** env) {
  char b = (*(SP) == SYM_TAG) && (*(SP+1) == T_SYM);
  SP+=2;
  if (b) {
    ulong a[4] = {*SP, *(SP+1), *(SP+2), *(SP+3)};
    *SP = a[2];
    *(SP+1) = a[3];
    *(SP+2) = a[0];
    *(SP+3) = a[1];
  }
}
void p_tag(ulong** env) {
  *(SP+1) = TAG_MASK(*SP);
  *SP = INT_TAG;
}
void p_read(ulong** env) {
  ulong* h = read();
  if (h == -1) {
    *(--SP) = -1;
    *(--SP) = CONS_TAG;
  } else {
    *(--SP) = *(h+1);
    *(--SP) = *(h);
  }
}
void p_print(ulong** env) {
  print(SP, 1);
  SP+=2;
}

void p_sstack(ulong** env) {
  for (ulong* a = (ulong*)(M+SSTART-16); a >= SP; a-=2) {
    print(a, 1);
  }
}
void p_env(ulong** env) {
  *(--SP) = SND(*env);
  *(--SP) = FST(*env);
}
void p_dup(ulong** env) {
  SP-=2;
  *SP = *(SP+2);
  *(SP+1) = *(SP+3);
}
void p_drop(ulong** env) {
  SP+=2;
}
void p_add(ulong** env) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Adding non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) += v;
}
void p_sub(ulong** env) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Subtracting non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) -= v;
}
void p_mul(ulong** env) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Multiplying non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) *= v;
}
void p_div(ulong** env) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Dividing non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) /= v;
}
void p_mod(ulong** env) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Modulo non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) %= v;
}
void p_lsh(ulong** env) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Left shift non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) <<= v;
}
void p_rsh(ulong** env) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Right shift non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) >>= v;
}
void p_nand(ulong** env) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Nand non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) = ~(*(SP+1) & v);
}

void p_load(ulong** env) {
  if (TAG_MASK(*SP) != INT_TAG) panic("Non-int in load");
  *(SP+1) = *((ulong*) *(SP+1));
}
void p_store(ulong** env) {
  if (TAG_MASK(*SP) != INT_TAG || TAG_MASK(*(SP+2)) != INT_TAG) panic("Non-int in store");
  ulong* addr = (ulong*) *(SP+1);
  SP+=2;
  *(SP+1) = *addr;
}
void p_obj_to_ptr(ulong** env) {
  *(SP+1) = (ulong) new_cons(*SP, *(SP+1));
  *SP = INT_TAG;
}
void p_ptr_to_obj(ulong** env) {
  if (TAG_MASK(*SP) != INT_TAG) panic("Non-int in ptr_to_obj");
  *SP = *((ulong*) *(SP+1));
  *(SP+1) = *(((ulong*) *(SP+1)) + 1);
}

ulong* env_define_prim(ulong* env, char* raw_sym, stack_func prim) {
  return new_cons(new_cons(new_cons(SYM_TAG, raw_sym),
                           new_cons(PRIM_TAG, prim)),
                  env);
}

void strcpy_inc(char** dest, char* src) {
  while (*((*dest)++) = *src++) {}
}


int forsp_main() {
  SP = M+SSTART;
  HP = M+HSTART;
  fromspace = HP;
  tospace = (ulong*)(((ulong)HP) + SEMIHEAPSIZE);

  char* dict = (char*)M;
  env = -1;
#define BAKE_DEF(cstr, prim)                                            \
  {                                                                     \
    env = env_define_prim(env, dict, prim);                             \
    strcpy_inc(&dict, cstr);                                            \
  }

  // strings for special syntax forms
  SQUOTE_SYM = dict;
  strcpy_inc(&dict, "'");
  SPUSH_SYM = dict;
  strcpy_inc(&dict, "$");
  SPOP_SET_SYM = dict;
  strcpy_inc(&dict, "^");
  SPOP_EXT_SYM = dict;
  strcpy_inc(&dict, ":");
  OPAREN_SYM = dict;
  strcpy_inc(&dict, "(");
  CPAREN_SYM = dict;
  strcpy_inc(&dict, ")");
  T_SYM = dict;
  strcpy_inc(&dict, "t");
  QUOTE_SYM = dict;
  strcpy_inc(&dict, "quote");

  // primitives
  PUSH_SYM = dict;
  BAKE_DEF("push", p_push);
  POP_SET_SYM = dict;
  BAKE_DEF("pops", p_pops);
  POP_EXT_SYM = dict;
  BAKE_DEF("pope", p_pope);

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

  DP = (ulong*)dict;

  read_char();                  // clear the dummy peek char

  while (1) {
    eval(&env, read());
  }
}

#ifndef BAREMETAL
int main(int argc, char** argv) {
  return forsp_main();
}
#endif
