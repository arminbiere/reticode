#include "disreti.h"

#include <stdlib.h>

int main(int argc, char ** argv) {
  srand (42);
  char str[disassembled_reti_code_length];
  unsigned pc = 0;
  do {
    unsigned code = (unsigned) rand () ^ (unsigned) (rand () << 16);
    if (disassemble_reti_code (code, str))
      printf ("%-21s ; %08x %08x\n", str, pc, code);
  } while (++pc);
  return 0;
}
