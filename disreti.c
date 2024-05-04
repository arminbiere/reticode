#include "disreti.h"

#include <stdlib.h>

int main() {
  srand (42);
  char str[disassembled_reti_code_length];
  for (;;) {
    unsigned code = (unsigned) rand () ^ (unsigned) (rand () << 16);
    (void) disassemble_reti_code (code, str);
    printf ("%08x %s\n", code, str);
  }
  return 0;
}
