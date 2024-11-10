/********************************************************************/
/*                 On reading and editing this file                 */
/*                                                                  */
/* The C in this file is extremely delicate. This file should be    */
/* compiled without optimizations, and seemingly quick fixes on the */
/* programmer's part should be considered *very* carefully. This is */
/* entirely because of new_cons. This is because new_cons returns   */
/* pointers to interally garbage collected regions of memory, the   */
/* lifetime of which follows the follwing rules:                    */
/*                                                                  */
/* A pointer to a cons cell (16 bytes) returned by new_cons is      */
/* valid until the next time garbage collection happens. If the     */
/* cons cell is referenced in a legal way (as a child of a cons     */
/* cell or a proc tagged cell) from a root, the cons cell survives  */
/* garbage collection and continues to be valid memory which will   */
/* not be re-alloced for another purpose. Garbage collection        */
/* happens when the heap runs out of space, which is checked on     */
/* *every* new_cons call.                                           */
/*                                                                  */
/* This means that when performing any sort of manipulation between */
/* valid static states of the abstract state machine,               */
/*                                                                  */
/* ONLY ONE VALUE RETURNED BY new_cons SHOULD BE FLOATING AT A TIME */
/*                                                                  */
/* Best practice is to place the floating value on the state        */
/* machine stack before calling new_cons again, as all valid stack  */
/* slots are roots. To facilitate the construction of linked data   */
/* between abstract state machine states, NULL (0) is a valid child */
/* pointer from cons cells, and will be ignored by the garbage      */
/* collector. Note that it is *not* a valid pointer for a cons      */
/* cell as it would be used by the abstract state machine. NIL has  */
/* its own representation.                                          */
/*                                                                  */
/* More details about the thought process behind this decision can  */
/* be found here: [gc.org]                                          */
/*                                                                  */
/* If reading this file can make even one rustacean feel ill, it    */
/* will all have been worth it. :P                                  */
/********************************************************************/

// #include <sys/cdefs.h>

// #define SANITY_CHECKS_ENABLED
#define BAREMETAL

#ifdef SANITY_CHECKS_ENABLED
#define SANITY(body)                            \
  body
#define SANITY_ALT(body, altbody)               \
  body
#else
#define SANITY(body)
#define SANITY_ALT(body, altbody)               \
  altbody
#endif

#define ASSERT(condition, msg)                  \
  if (!(condition)) panic(msg)

#ifdef BAREMETAL
#include "riscv.h"
#endif

#ifndef BAREMETAL
#include <stdio.h>
void putstring(char* s) {fputs(s, stdout);}
#else
extern char getchar(void);
extern void putchar(char);
extern void putstring(char*);
#endif

#ifndef BAREMETAL
#ifdef SANITY_CHECKS_ENABLED
#include <string.h>
#endif
#endif

typedef unsigned long long ulong;
_Static_assert (sizeof(ulong) == 8, "ulong isn't a 8byte word");
_Static_assert (sizeof(ulong*) == 8, "ulong* isn't a 8byte word");

#define RND_UP(a) ((ulong*) (((ulong)((a)+0xf)) & (~0xfULL)))
#define ADDR_MASK(a) ((ulong*) ((ulong) (a) & (~0xfULL)))
#define TAG_MASK(a) (((ulong) (a)) & 0xf)
#define FST(a) (ADDR_MASK(a)[0])
#define SND(a) (ADDR_MASK(a)[1])

#define MAX_PRINT_DEPTH 8

#define MEMSIZE 0x200000
// ^ Approx 2MB
#ifdef SANITY_CHECKS_ENABLED
const ulong MIDPOINT = (MEMSIZE/2);
const ulong DSTART = 0;
const ulong SSTART = (MIDPOINT - 16);
const ulong HSTART = (MIDPOINT);
const ulong HEAPSIZE = (MEMSIZE/2);
const ulong SEMIHEAPSIZE = (HEAPSIZE/2);
const ulong STACKSIZE = (MEMSIZE/2);
#else
#define MIDPOINT (MEMSIZE/2)
#define DSTART 0
#define SSTART (MIDPOINT - 16)
#define HSTART (MIDPOINT)
#define HEAPSIZE (MEMSIZE/2)
#define SEMIHEAPSIZE (HEAPSIZE/2)
#define STACKSIZE (MEMSIZE/2)
#endif

/* must be 16byte aligned */
#ifdef BAREMETAL
extern char* MAINMEM;
char *M;
#else
char M[MEMSIZE];
#endif


ulong *SP, *root_env;
char *DP;

typedef struct cell {
  ulong car;
  ulong cdr;
} cell;
typedef void (*stack_func)(void);

#define CONS_TAG   0
#define SYM_TAG    1
#define INT_TAG    2
#define PROC_TAG   3
#define PRIM_TAG   4
#define GC_FWD_TAG 5
#define NIL_TAG    6

cell return_stack = {NIL_TAG,0};
ulong depth = 0;
cell read_stack;

#define PROC (ADDR_MASK(return_stack.car))
#define ENV (&SND(PROC))
#define BODY (FST(PROC))
#define CUR (FST(BODY))
#define INC_PC FST(return_stack.car) = (ulong)(SND(FST(return_stack.car))) | PROC_TAG

#define INSTALL(cnt)                                                    \
  /* expects a proc */                                                  \
  /* explicity copy of the children of cnt to prevent mutation */       \
  SANITY(ASSERT(TAG_MASK(FST(cnt)) == PROC_TAG, "non-proc in install"));\
  ulong _a = FST(cnt), _b = SND(cnt);                                   \
  /* ^ safe if cnt is unreachable or on the stack */                    \
  PUSH(_a, _b);                                                         \
  if ((return_stack.car != NIL_TAG) &&                                  \
      (FST(SND(BODY)) == NIL_TAG) &&                                    \
      (FST(return_stack.cdr) != NIL_TAG)) {                             \
    /* tail call and not the root or first call */                      \
    ulong* h = new_cons(FST(SP), SND(SP));                              \
    return_stack.car = h;                                               \
    SP+=2;                                                              \
  } else {                                                              \
  ulong* h = new_cons(SP[0], SP[1]);                                    \
  SP[0] = (ulong)h | CONS_TAG;                                          \
  h = new_cons(return_stack.car, return_stack.cdr);                     \
  SP[1] = h;                                                            \
  ++depth;                                                              \
  return_stack.car = FST(SP);                                           \
  return_stack.cdr = SND(SP);                                           \
  SP+=2;                                                                \
  }

#define PUSH(a,b)                               \
  *(--SP) = b;                                  \
  *(--SP) = a

#ifdef BAREMETAL
extern void print_err(char*);
extern void panic(char* msg);
#else
void print_err(char* msg) {fputs(msg, stderr);}
void panic(char* msg) {
  print_err(msg);
  while (1) {}
}
#endif

SANITY(char* logfilename = "mdump.dot";
       FILE* logfilefd;

       void emit_node(ulong* obj, char* color) {
         switch (TAG_MASK(FST(obj))) {
         case SYM_TAG:
           fprintf(logfilefd, "\"%llx\" [color=%s, label=\"%s\"]\n", obj, color, (char*)SND(obj));
           break;
         case INT_TAG:
           fprintf(logfilefd, "\"%llx\" [color=%s, label=\"INT %llx\"]\n", obj, color, SND(obj));
           break;
         case PRIM_TAG:
           fprintf(logfilefd, "\"%llx\" [color=%s, label=\"PRIM\"]\n", obj, color);
           break;
         case NIL_TAG:
           fprintf(logfilefd, "\"%llx\" [color=%s, label=\"NIL\"]\n", obj, color);
           break;
         case CONS_TAG:
         case PROC_TAG:
           fprintf(logfilefd, "\"%llx\" [color=%s, label=\"%s %llx\"]\n",
                   obj,
                   color,
                   (TAG_MASK(FST(obj)) == CONS_TAG) ? "CONS" : "PROC",
                   obj);
           break;
         case GC_FWD_TAG:
           panic("emit called on garbage collection forward pointer!");
         }
       }
       )
ulong *tospace, *fromspace, *HP;
ulong* copy(ulong* obj) {
  if (HP+2 >= fromspace + (SEMIHEAPSIZE / sizeof(ulong))) panic("OOM!\n");
  if (!obj) return obj;         /* NULL is valid */
  if (TAG_MASK(FST(obj)) == GC_FWD_TAG) return SND(obj);
  SANITY(
         fprintf(logfilefd, "subgraph {\n");
         fprintf(logfilefd, "subgraph {\n");
         emit_node(obj, "red");
         );
  ulong* newaddr = HP;
  FST(HP) = FST(obj);
  SND(HP) = SND(obj);
  HP += 2;
  FST(obj) = GC_FWD_TAG;
  SND(obj) = newaddr;

  SANITY(
         emit_node(newaddr, "black");
         fprintf(logfilefd, "\"%llx\" -> \"%llx\" [color=green];\n}\n",
                 (ulong)obj,
                 (ulong)newaddr);
         ulong tag = TAG_MASK(FST(newaddr));
         if (tag == CONS_TAG || tag == PROC_TAG) {
           fprintf(logfilefd, "\"%llx\" -> \"%llx\" [color=\"red:magenta\"];\n",
                   (ulong)obj,
                   ADDR_MASK(FST(newaddr)));
           fprintf(logfilefd, "\"%llx\" -> \"%llx\" [color=\"red:royalblue\"];\n",
                   (ulong)obj,
                   ADDR_MASK(SND(newaddr)));
         }
         fprintf(logfilefd, "}\n");
         );
  return newaddr;
}

void collect() {
  SANITY(
         print_err("GC!\n");
         logfilefd = fopen(logfilename, "w+");
         fwrite("digraph {\n", 1, 10, logfilefd);
         );
  ulong* scan;
  ulong* hold = fromspace;
  fromspace = tospace;
  tospace = hold;
  HP = fromspace;
  scan = fromspace;

  SANITY(
         fprintf(logfilefd, "subgraph {\n");
         fprintf(logfilefd, "subgraph {\n");
         fprintf(logfilefd, "root_env;\nroot_env -> \"%llx\" [color=red];\n", root_env);
         );
  root_env = copy(root_env);
  SANITY(
         fprintf(logfilefd, "root_env -> \"%llx\";\n", root_env);
         fprintf(logfilefd, "}\n");
         fprintf(logfilefd, "subgraph {\n");
         );
  if (TAG_MASK(read_stack.car) == CONS_TAG) {
    SANITY(fprintf(logfilefd, "readstack;\nreadstack -> \"%llx\" [color=\"red:magenta\"];\n", read_stack.car));
    SANITY(fprintf(logfilefd, "readstack -> \"%llx\" [color=\"red:royalblue\"];\n", read_stack.cdr));
    read_stack.car = copy(read_stack.car);
    read_stack.cdr = copy(read_stack.cdr);
    SANITY(fprintf(logfilefd, "readstack -> \"%llx\" [color=\"black:magenta\"];\n", read_stack.car));
    SANITY(fprintf(logfilefd, "readstack -> \"%llx\" [color=\"black:royalblue\"];\n", read_stack.cdr));
  }

  if (TAG_MASK(return_stack.car) == CONS_TAG) {
    SANITY(fprintf(logfilefd, "return_stack;\nreturn_stack -> \"%llx\" [color=\"red:magenta\"];\n", return_stack.car));
    SANITY(fprintf(logfilefd, "return_stack -> \"%llx\" [color=\"red:royalblue\"];\n", return_stack.cdr));
    return_stack.car = copy(return_stack.car);
    return_stack.cdr = copy(return_stack.cdr);
    SANITY(fprintf(logfilefd, "return_stack -> \"%llx\" [color=\"black:magenta\"];\n", return_stack.car));
    SANITY(fprintf(logfilefd, "return_stack -> \"%llx\" [color=\"black:royalblue\"];\n", return_stack.cdr));
  }

  SANITY(
         fprintf(logfilefd, "}\n");
         fprintf(logfilefd, "subgraph {\n");
         for (ulong* a = (ulong*)(M+SSTART-16); a >= (ulong*)SP; a-=2) {
           ulong idx = (a-SP)/2;
           fprintf(logfilefd, "\"stack%llx\";\n", idx);
           if (idx != 0)
             fprintf(logfilefd, "\"stack%llx\" -> \"stack%llx\" [color=indigo];\n", idx, idx-1);
           if (ADDR_MASK(FST(a)) != 0)
             fprintf(logfilefd, "\"stack%llx\" -> \"%llx\" [color=red];\n",
                     idx,
                     (ulong) ADDR_MASK(FST(a)));
           if (ADDR_MASK(SND(a)) != 0)
             fprintf(logfilefd, "\"stack%llx\" -> \"%llx\" [color=red];\n",
                     idx,
                     (ulong) ADDR_MASK(SND(a)));
         }
         fprintf(logfilefd, "}\n");
         );
  for (ulong* a = (ulong*)(M+SSTART-16); a >= (ulong*)SP; a-=2) {
    ulong tag = TAG_MASK(FST(a));
    ulong idx = (a-SP)/2;
    switch (tag) {
    case CONS_TAG:
    case PROC_TAG:
      SANITY(
             if (ADDR_MASK(FST(a)) != 0)
               fprintf(logfilefd, "\"stack%llx\" -> \"%llx\" [color=\"red:magenta\"];\n",
                       idx,
                       (ulong) ADDR_MASK(FST(a)));
             if (ADDR_MASK(SND(a)) != 0)
               fprintf(logfilefd, "\"stack%llx\" -> \"%llx\" [color=\"red:royalblue\"];\n",
                       idx,
                       (ulong) ADDR_MASK(SND(a)));
             );
      FST(a) = (ulong)copy(ADDR_MASK(FST(a))) | tag;
      SND(a) = copy(SND(a));
      SANITY(
             if (ADDR_MASK(FST(a)) != 0)
               fprintf(logfilefd, "\"stack%llx\" -> \"%llx\" [color=\"black:magenta\"];\n",
                       idx,
                       (ulong) ADDR_MASK(FST(a)));
             if (ADDR_MASK(SND(a)) != 0)
               fprintf(logfilefd, "\"stack%llx\" -> \"%llx\" [color=\"black:royalblue\"];\n",
                       idx,
                       (ulong) ADDR_MASK(SND(a)));
             );
      break;
    }
  }
  SANITY(fprintf(logfilefd, "}\n"));

  while (scan < HP) {
    ulong tag = TAG_MASK(FST(scan));
    switch (tag) {
    case CONS_TAG:
    case PROC_TAG:
      SANITY(
       if (ADDR_MASK(FST(scan)) != 0)
         fprintf(logfilefd, "\"%llx\" -> \"%llx\" [color=\"red:magenta\"];\n",
                 scan,
                 (ulong) ADDR_MASK(FST(scan)));
       if (ADDR_MASK(SND(scan)) != 0)
         fprintf(logfilefd, "\"%llx\" -> \"%llx\" [color=\"red:royalblue\"];\n",
                 scan,
                 (ulong) ADDR_MASK(SND(scan)));
             );
      FST(scan) = (ulong)copy(ADDR_MASK(FST(scan))) | tag;
      SND(scan) = copy(SND(scan));
      SANITY(
       if (ADDR_MASK(FST(scan)) != 0)
         fprintf(logfilefd, "\"%llx\" -> \"%llx\" [color=\"black:magenta\"];\n",
                 scan,
                 (ulong) ADDR_MASK(FST(scan)));
       if (ADDR_MASK(SND(scan)) != 0)
         fprintf(logfilefd, "\"%llx\" -> \"%llx\" [color=\"black:royalblue\"];\n",
                 scan,
                 (ulong) ADDR_MASK(SND(scan)));
             );
      break;
    default:
      break;
    }
    scan += 2;
  }
  SANITY(
         fwrite("}", 1, 1, logfilefd);
         fclose(logfilefd);
         );
}

/* forces the evalutation of the arguments to come after the call that
   can trigger GC */
#define new_cons(a,b)                                                   \
  ({ulong* _hold = _new_cons(0,0); FST(_hold) = a, SND(_hold) = b, _hold;})
ulong* _new_cons(ulong a, ulong b) {
  if (HP + 2 >= ((ulong)fromspace) + SEMIHEAPSIZE) {
    collect();
  }
  if (HP + 2 >= ((ulong)fromspace) + SEMIHEAPSIZE) {
    panic("OOM!\n");
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
  SANITY(const char* original = a;)
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
#ifndef BAREMETAL
  SANITY(ASSERT(len == strlen(original), "Custom streq doesns't do what it should");)
#endif
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
char* PUSHR_SYM;
char* OPAREN_SYM;
char* CPAREN_SYM;
char* T_SYM;
char* READ_SYM;
char* READ_FLUSH_SYM;
char* PRINT_SYM;
char* OR_SYM;
char* CONS_SYM;

void p_push (void);
void p_pushr (void);
void p_pops (void);
void p_pope (void);
cell read_stack = {NIL_TAG,0};
/* DO NOT TOUCH */
#define STARTREADSTACK()                        \
  {                                             \
    PUSH(read_stack.car, read_stack.cdr);       \
  }

#define PUSHREADSTACK(contents)                                 \
  {                                                             \
    *(SP-1) = new_cons(*SP, *(SP+1));                           \
    *(SP-2) = CONS_TAG;                                         \
    SP-=2;                                                      \
    cell _h = contents;                                         \
    FST(SP) = (ulong)new_cons(_h.car, _h.cdr) | CONS_TAG;       \
    *(SP+2) = *SP;                                              \
    *(SP+3) = *(SP+1);                                          \
    SP+=2;                                                      \
  }

#define SAVEREADSTACK()                         \
  {                                             \
    read_stack.car = *SP;                       \
    read_stack.cdr = *(SP+1);                   \
    SP+=2;                                      \
  }

SANITY(ulong read_depth = 0;)
cell read() {
  SANITY(++read_depth; ulong* entry_SP = SP;)
    cell ret;
  if (read_stack.car != NIL_TAG) {
    ulong* h = read_stack.car;
    ret.car = FST(h);
    ret.cdr = SND(h);
    read_stack.car = FST(read_stack.cdr);
    read_stack.cdr = SND(read_stack.cdr);
    ret = ret;
  } else {
    slurp_whitespace();
    char* raw_sym = read_token();
    if (raw_sym == SQUOTE_SYM) {
      cell out = {SYM_TAG, QUOTE_SYM};
      ret = out;
    } else if (raw_sym == SQUOTE_SYM) {
      STARTREADSTACK();
      PUSHREADSTACK(read());
      SAVEREADSTACK();
      cell out = {SYM_TAG, QUOTE_SYM};
      ret = out;
    } else if (raw_sym == SPUSH_SYM) {
      STARTREADSTACK();
      cell rs = {SYM_TAG, PUSH_SYM};
      PUSHREADSTACK(rs);
      PUSHREADSTACK(read());
      SAVEREADSTACK();
      cell out = {SYM_TAG, QUOTE_SYM};
      ret = out;
    } else if (raw_sym == SPOP_SET_SYM) {
      STARTREADSTACK();
      cell rs = {SYM_TAG, POP_SET_SYM};
      PUSHREADSTACK(rs);
      PUSHREADSTACK(read());
      SAVEREADSTACK();
      cell out = {SYM_TAG, QUOTE_SYM};
      ret = out;
    } else if (raw_sym == SPOP_EXT_SYM) {
      STARTREADSTACK();
      cell rs = {SYM_TAG, POP_EXT_SYM};
      PUSHREADSTACK(rs);
      PUSHREADSTACK(read());
      SAVEREADSTACK();
      cell out = {SYM_TAG, QUOTE_SYM};
      ret = out;
    } else if (raw_sym == OPAREN_SYM) {
      /* This is weird but forces the allocation to happen in order,
         interleaved with placing the location on the stack so it is
         reachable. */
      ulong* stack_marker = SP;
      ret = read();
      if (ret.car == NIL_TAG)  ret = ret;
      while (ret.car != NIL_TAG) {
        SP-=2;
        *SP = ret.car;
        *(SP+1) = ret.cdr;
        ulong* h = new_cons(0, 0);
        FST(h) = *SP | CONS_TAG;
        SND(h) = *(SP+1);
        *(SP+1) = 0;
        *SP = h;
        *(SP+1) = new_cons(NIL_TAG, 0);
        ret = read();
      }
      // The list is read into the stack, now collect it up
      while (SP < stack_marker-2) {
        *(SP+3) = new_cons(*(SP), *(SP+1));
        SP += 2;
      }
      cell out = {FST(SP), SND(SP)};
      SP+=2;
      ret = out;
    } else if (raw_sym == CPAREN_SYM) {
      cell out = {NIL_TAG, 0};
      ret = out;
    } else {
      char* cstr = raw_sym;
      struct as_int_t maybe_int = as_int(cstr);
      if (maybe_int.b) {        // C struct return type moment :( ugly
        cell out = {INT_TAG, maybe_int.v};
        ret = out;
      } else {
        cell out = {SYM_TAG, raw_sym};
        ret = out;
      }
    }
  }
  SANITY(--read_depth; if (!read_depth) ASSERT(SP == entry_SP, "Non recursive read altered SP");)
    return ret;
}

ulong print_depth = 0;
void print(ulong*, char);
void print_list(ulong* l) {
  if (!l) panic("NULL head in print_list");
  while ((TAG_MASK(FST(l)) != NIL_TAG) && (TAG_MASK(FST(l)) == CONS_TAG)) {
    print(FST(l), 0);
    if (FST(SND(l)) != NIL_TAG) putchar(' ');
    l = SND(l);
  }
}
#define pow(a,b) {typeof(a) _val=1; typeof(b) _b = b; while(_b--) _val*=a; _val;}
void print_int(ulong val) {
  if ((long long) val < 0) putchar('-');
  val &= ~(1ULL << 63);
  for (int s = 19; s >= 0; --s) {
    int c = (val / (pow(10, s))) % 10;
    putchar((char)(c + '0'));
  }
}
void print(ulong* v, char newline) {
  if (print_depth >= MAX_PRINT_DEPTH) {
    putstring("...");
    return;
  }
  ++print_depth;
  switch (TAG_MASK(FST(v))) {
  case NIL_TAG:
    putstring("nil");
    break;
  case CONS_TAG:
    if (TAG_MASK(FST(SND(v))) != CONS_TAG) {
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
    print_list(FST(v));
    putchar(']');
    break;
  case PRIM_TAG:
    putstring("PRIM");
    break;
  default:
    panic("Unknown tag in print!");
  }
  if (newline) putchar('\n');
  --print_depth;
}

ulong* lookup(ulong* env, char* raw_sym) {
  if (!env) panic("NULL env in lookup!");
  while (TAG_MASK(FST(env)) == CONS_TAG) {
    ulong* pair = FST(env);
    ulong* cursym = FST(pair);
    ulong _len;
    if (streq(&_len, (char*)SND(cursym), raw_sym)) {
      return SND(pair);
    }
    env = SND(env);
  }
  if (TAG_MASK(FST(env)) != NIL_TAG) panic("Malformed env in lookup!");
  print_err((char*)raw_sym);
  panic(": undefined symbol (lookup)!");
}


void eval() {
 eval_outer:
  while (TAG_MASK(return_stack.car) != NIL_TAG) {
    SANITY(
           ASSERT(TAG_MASK(return_stack.car) == CONS_TAG,
                  "Malformed return stack in eval!");
           ASSERT(TAG_MASK(FST(PROC)) == PROC_TAG,
                  "Non proc on return stack!");
           );
    if (TAG_MASK(FST(BODY)) == NIL_TAG) {
      // exhausted the body of the procedure, pop from ret stack
      --depth;
      if (depth == 0) root_env = *ENV;
      return_stack.car = FST(return_stack.cdr);
      return_stack.cdr = SND(return_stack.cdr);
      if (depth == 0) return;
      else {
        INC_PC;
        continue;
      }
    }
    ASSERT(SP-2 > DP, "Stack overflow!");
    SANITY(ASSERT(CUR, "NULL in eval!"));

    switch (TAG_MASK(FST(CUR))) {
    case NIL_TAG:
    case INT_TAG:
      PUSH(FST(CUR), SND(CUR));
      break;
    case CONS_TAG:
      PUSH((ulong)CUR | PROC_TAG, (ulong)(*ENV));
      break;
    case SYM_TAG:
      if (SND(CUR) == QUOTE_SYM) {
        INC_PC;
        ASSERT(TAG_MASK(FST(BODY)) != NIL_TAG,
               "No data followed a quote in eval!");
        PUSH(FST(CUR), SND(CUR));
      } else {
        /* act on interal value */
        ulong* val = lookup(*ENV, SND(CUR));
        switch (TAG_MASK(FST(val))) {
        case NIL_TAG:
        case INT_TAG:
          PUSH(FST(val), SND(val));
          break;
        case CONS_TAG:
          PUSH((ulong)val | PROC_TAG, (ulong)(*ENV));
          break;
        case SYM_TAG:
          /* act as if quoted */
          PUSH(FST(val), SND(val));
          break;
        case PROC_TAG:
          INSTALL(val);
          goto eval_outer;
        case PRIM_TAG:
          if (((stack_func)(SND(val))) == p_pushr) {
            /* INC_PC happens internally */
            ((stack_func)(SND(val)))();
            goto eval_outer;
          } else {
            ((stack_func)(SND(val)))();
            break;
          }
        default:
          panic("Unknown tag in eval while executing symbol!");
        }
      }
      break;
    case PROC_TAG:
      {
        INSTALL(CUR);
        continue;
      }
    case PRIM_TAG:
      if (((stack_func)(SND(CUR))) == p_pushr) {
        /* INC_PC happens internally */
        ((stack_func)(SND(CUR)))();
        goto eval_outer;
      } else {
        ((stack_func)(SND(CUR)))();
        break;
      }
    default:
      panic("Unknown tag in eval!");
    }
    // move the body pointer of the current procedure forward
    INC_PC;
  }
}

void p_push (void) {
  if (*SP != SYM_TAG) panic("push on non-sym!");
  ulong* h = lookup(*ENV, *(SP+1));
  *(SP+1) = *(h+1);
  *SP = *h;
}
void p_pope (void) {
  if (*SP != SYM_TAG) panic("pope on non-sym!");
  ulong* s = new_cons(*SP, *(SP+1));
  *(SP) = (ulong) s | CONS_TAG;
  *(SP+1) = 0;
  ulong* tmp = new_cons(*(SP+2), *(SP+3));
  *(SP+1) = tmp;
  *(SP+2) = 0 | CONS_TAG;
  *(SP+3) = 0;
  *(SP+3) = new_cons(*SP, *(SP+1));
  *(SP+2) = new_cons(*(SP+3), *ENV);
  *ENV = *(SP+2);
  SP+=4;
}
void p_pops (void) {
  if (*SP != SYM_TAG) panic("pops on non-sym!");
  char* target = (char*)SP[1];
  ulong* s = new_cons(*SP, *(SP+1));
  *(SP) = (ulong)s | CONS_TAG;
  *(SP+1) = 0;
  ulong* val = new_cons(*(SP+2), *(SP+3));
  *(SP+1) = val;
  ulong* env = *ENV;
  while (TAG_MASK(FST(env)) == CONS_TAG) {
    ulong* pair = FST(env);
    ulong* cursym = FST(pair);
    ulong _len;
    if (streq(&_len, target, (char*)SND(cursym))) {
      SND(pair) = val;
      SP+=4;
      return;
    }
    env = SND(env);
  }
  if (TAG_MASK(FST(env)) != NIL_TAG) panic("Mallformed env in set!");
  print_err((char*)*(SP+1));
  panic(": undefined symbol (set)!");
}

void p_pushr (void) {
  if (TAG_MASK(*SP) != PROC_TAG) panic("pushr on non-proc!");
  /* expects a proc */                                                  \
  /* explicity copy of the children of cnt to prevent mutation */       \
  ulong* h = new_cons(SP[0], SP[1]);                                    \
  SP[0] = (ulong)h | CONS_TAG;                                          \
  h = new_cons(return_stack.car, return_stack.cdr);                     \
  SP[1] = h;                                                            \
  ++depth;                                                              \
  return_stack.car = FST(SP);                                           \
  return_stack.cdr = SND(SP);                                           \
  SP+=2;
}
void p_popr (void) {
  return_stack.car = FST(return_stack.cdr);
  return_stack.cdr = SND(return_stack.cdr);
  --depth;
}

/*
 * void p_captureroot (void) {
 *   /\* Transforms a cons cell on the stack into a proc in the root environment *\/
 *   ulong* p = new_cons(*SP,*(SP+1));
 *   FST(SP) = (ulong)p | PROC_TAG;
 *   SND(SP) = root_env;
 * }
 */

void p_eq (void) {
  char r = (*SP == *(SP+2)) && (*(SP+1) == *(SP+3));
  SP+=2;
  if (r) {
    *SP = SYM_TAG;
    *(SP+1) = T_SYM;
  } else {
    *SP = NIL_TAG;
    *(SP+1) = 0;
  }
}
void p_cons (void) {
  ulong* car = new_cons(*SP, *(SP+1));
  *SP = CONS_TAG;
  *(SP+1) = car;
  ulong* cdr = new_cons(*(SP+2), *(SP+3));
  SP+=2;
  *SP = ((ulong)car) | CONS_TAG;
  *(SP+1) = cdr;
}
void p_car (void) {
  if (TAG_MASK(*SP) != CONS_TAG) panic("Non-cons in car!");
  ulong* h = ADDR_MASK(*SP);
  *SP = FST(h);
  *(SP+1) = SND(h);
}
void p_cdr (void) {
  if (TAG_MASK(SP) != CONS_TAG) panic("Non-cons in car!");
  ulong* h = *(SP+1);
  *SP = FST(h);
  *(SP+1) = SND(h);
}
void p_cswap (void) {
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
void p_tag (void) {
  *(SP+1) = TAG_MASK(*SP);
  *SP = INT_TAG;
}
void p_read (void) {
  cell h = read();
  PUSH(h.car, h.cdr);
}
void p_print (void) {
  print(SP, 1);
  SP+=2;
}

void p_sstack (void) {
  for (ulong* a = (ulong*)(M+SSTART-16); a >= SP; a-=2) {
    print(a, 1);
  }
}
void p_env (void) {
  PUSH(FST(*ENV), SND(*ENV));
}
void p_dup (void) {
  SP-=2;
  *SP = *(SP+2);
  *(SP+1) = *(SP+3);
}
void p_drop (void) {
  SP+=2;
}
void p_add (void) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Adding non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) += v;
}
void p_sub (void) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Subtracting non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) -= v;
}
void p_mul (void) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Multiplying non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) *= v;
}
void p_div (void) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Dividing non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) /= v;
}
void p_mod (void) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Modulo non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) %= v;
}
void p_lsh (void) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Left shift non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) <<= v;
}
void p_rsh (void) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Right shift non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) >>= v;
}
void p_nand (void) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Nand non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) = ~(*(SP+1) & v);
}
void p_or (void) {
  if ((TAG_MASK(*SP) != INT_TAG) || (TAG_MASK(*(SP+2)) != INT_TAG)) panic("Nand non-ints!");
  ulong v = *(SP+1);
  SP+=2;
  *(SP+1) = *(SP+1) | v;
}

void p_load (void) {
  if (TAG_MASK(*SP) != INT_TAG) panic("Non-int in load");
  *(SP+1) = *((ulong*) *(SP+1));
}
void p_store (void) {
  if (TAG_MASK(*SP) != INT_TAG || TAG_MASK(*(SP+2)) != INT_TAG) panic("Non-int in store");
  ulong* addr = (ulong*) *(SP+1);
  *addr = *(SP+3);
  SP+=4;
}
void p_load_b (void) {
  if (TAG_MASK(*SP) != INT_TAG) panic("Non-int in load");
  *(SP+1) = *((unsigned char*) *(SP+1));
}
void p_store_b (void) {
  if (TAG_MASK(*SP) != INT_TAG || TAG_MASK(*(SP+2)) != INT_TAG) panic("Non-int in store");
  unsigned char* addr = (unsigned char*) *(SP+1);
  *addr = *(unsigned char*)(SP+3);
  SP+=4;
}
void p_load_2b (void) {
  if (TAG_MASK(*SP) != INT_TAG) panic("Non-int in load");
  *(SP+1) = *((unsigned short*) *(SP+1));
}
void p_store_2b (void) {
  if (TAG_MASK(*SP) != INT_TAG || TAG_MASK(*(SP+2)) != INT_TAG) panic("Non-int in store");
  unsigned short* addr = (unsigned short*) *(SP+1);
  *addr = *(unsigned short*)(SP+3);
  SP+=4;
}
void p_load_4b (void) {
  if (TAG_MASK(*SP) != INT_TAG) panic("Non-int in load");
  *(SP+1) = *((unsigned int*) *(SP+1));
}
void p_store_4b (void) {
  if (TAG_MASK(*SP) != INT_TAG || TAG_MASK(*(SP+2)) != INT_TAG) panic("Non-int in store");
  unsigned int* addr = (unsigned int*) *(SP+1);
  *addr = *(unsigned int*)(SP+3);
  SP+=4;
}

ulong* env_define_prim(ulong* env, char* raw_sym, stack_func prim) {
  // FOR USE ONLY IN STARTUP. NOT GC SAFE
  return new_cons(new_cons(new_cons(SYM_TAG, raw_sym),
                           new_cons(PRIM_TAG, prim)),
                  env);
}

void strcpy_inc(char** dest, char* src) {
  while (*((*dest)++) = *src++) {}
}

int forsp_main() {
  M = &MAINMEM;
  ASSERT(((ulong)M & 0xf) == 0, "Memory base isn't 16byte aligned!");

  SP = M+SSTART;
  HP = M+HSTART;
  fromspace = HP;
  tospace = (ulong*)(((ulong)HP) + SEMIHEAPSIZE);

  char* dict = (char*)M;
  root_env = new_cons(NIL_TAG, 0);
#define BAKE_DEF(cstr, prim)                            \
  {                                                     \
    root_env = env_define_prim(root_env, dict, prim);   \
    strcpy_inc(&dict, cstr);                            \
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
  PUSHR_SYM = dict;
  BAKE_DEF("pushr", p_pushr);
  BAKE_DEF("popr", p_popr);
  /*
   * ulong* mkproc_sym = dict;
   * BAKE_DEF("captureroot", p_captureroot);
   */

  BAKE_DEF("eq", p_eq);
  CONS_SYM = dict;
  BAKE_DEF("cons", p_cons);
  BAKE_DEF("car", p_car);
  BAKE_DEF("cdr", p_cdr);
  BAKE_DEF("cswap", p_cswap);
  BAKE_DEF("tag", p_tag);
  READ_SYM = dict;
  BAKE_DEF("read", p_read);
  PRINT_SYM = dict;
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
  BAKE_DEF("or", p_or);

  BAKE_DEF("load", p_load);
  BAKE_DEF("store", p_store);
  BAKE_DEF("load_b", p_load_b);
  BAKE_DEF("store_b", p_store_b);
  BAKE_DEF("load_2b", p_load_2b);
  BAKE_DEF("store_2b", p_store_2b);
  BAKE_DEF("load_4b", p_load_4b);
  BAKE_DEF("store_4b", p_store_4b);

  DP = dict;

  read_char();                  // clear the dummy peek char


  // not really a repl since it doesn't print anything
  /*
   * ulong* repl_read = new_cons(SYM_TAG, READ_FLUSH_SYM);
   * ulong* repl_mkproc = new_cons(SYM_TAG, mkproc_sym);
   * ulong* repl_pushr = new_cons(SYM_TAG, PUSHR_SYM);
   * ulong* repl_body = new_cons(repl_read,
   *                             new_cons(repl_mkproc,
   *                                      new_cons(repl_pushr,
   *                                               0)));
   * SND(SND(SND(repl_body))) = repl_body; // cyclical
   * return_stack.car = new_cons((ulong)repl_body | PROC_TAG, root_env) ;
   * return_stack.cdr = new_cons(NIL_TAG,0);
   */

  return_stack.car = NIL_TAG;
  return_stack.cdr = 0;

  /* Begin! */
  while (1) {
    {
      /* Returns list of reads guarenteed to empty read_stack. */
      cell cur;
      ulong* stack_marker = SP-2;
      do {
        cur = read();
        PUSH(cur.car, cur.cdr);
      } while (TAG_MASK(read_stack.car) != NIL_TAG);
      PUSH(NIL_TAG, 0);
      while (SP != stack_marker) {
        ulong* tail = new_cons(*SP, *(SP+1));
        *SP = CONS_TAG;
        *(SP+1) = tail;
        tail = new_cons(*(SP+2), *(SP+3));
        *SP = (ulong)tail | CONS_TAG;
        *(SP+2) = *SP;
        *(SP+3) = *(SP+1);
        SP+=2;
      }
      /* The stack contains one new element, a list of the reads in order */
    }
    {
      /* Transforms a cons cell on the stack into a proc in the root environment */
      ulong* p = new_cons(*SP,*(SP+1));
      FST(SP) = (ulong)p | PROC_TAG;
      SND(SP) = root_env;
      /* top of stack is the proc representing the next set of inputs */
    }
    /* place the new instructions on the return stack */
    p_pushr();
    /* and do something with them */
    eval();
  }
}

#ifndef BAREMETAL
int main(int argc, char** argv) {
  return forsp_main();
}
#endif
