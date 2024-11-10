#include "riscv.h"

extern char* BM_TEXT;

char* BM_TEXT_PTR;

char getchar(void) {
  /* End of transmision */
  if (*BM_TEXT_PTR == 0x4) return 0x4;
  else return *(BM_TEXT_PTR++);
}

#define RINGBUFLEN 0x100
char ringbuf[RINGBUFLEN] = {' '};
int rb_ptr = 0;

void putchar(char c) {
  ringbuf[rb_ptr] = c;
  rb_ptr = (rb_ptr + 1) % RINGBUFLEN;
}

void putstring(char* msg) {
  while (*msg) {
    putchar(*msg);
    ++msg;
  }
}

void print_err(char* msg) {
  while (*msg) {
    putchar(*msg);
    ++msg;
  }
}

void panic(char* msg) {
  print_err("PANIC!\n");
  print_err(msg);
  while (1) {}
}
