/* Link against to provide the backend for base.c when running in
   64bit linux. */

#include <stdio.h>
#include <stdlib.h>

typedef int cell;

extern void setup(void);
extern cell eval(cell,cell);
extern void print(cell);
extern cell read(void);

const char inner_EOF = EOF;

void inner_flush() {
  fflush(stdout);
}

int inner_getchar() {
  return getchar();
}

void inner_putchar(char c) {
  putchar(c);
}

void print_ln() {inner_putchar('\n');}

typedef unsigned long num;
void inner_put_num(num n) {
  for (int i = 60; i >= 0; i-=4) {
    char c = ((n >> i) & 0x0f) + '0';
    c = c > '9' ? (c - '0' + 'a') : c;
    inner_putchar(c);
  }
}

void* inner_malloc(unsigned long sz) {
  return malloc(sz);
}

void vm_exit(unsigned long n) {
  exit(n);
}

int main() {
  // TODO make this smart with the defs in base
  setup();
  while (1) {
    print(eval(read(), 0));
    print_ln();
  }
}

