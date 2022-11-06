#define PUT_LBIT(p, n, v)   (*(unsigned int *)(p) = (*p & ~(1 << n)) | (v << n))
#include <stdio.h>

int main() {
  unsigned long x = 0x801;
  void* xp = &x;

  printf("%x\n", PUT_LBIT(xp, 1, 0));
  return 0;
}