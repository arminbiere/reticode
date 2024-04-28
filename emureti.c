#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define CAPACITY ((size_t)1 << 32)

static void die(const char *fmt, ...) {
  fputs("emureti: error: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

static bool file_exists(const char *path) {
  struct stat buf;
  return !stat(path, &buf);
}

#define BV2(B1, B0) ((B1 << 1) | (B0 << 0))

#define BV4(B3, B2, B1, B0) ((B3 << 3) | (B2 << 2) | (B1 << 1) | (B0 << 0))

#define BV5(B4, B3, B2, B1, B0)                                                \
  ((B4 << 4) | (B3 << 3) | (B2 << 2) | (B1 << 1) | (B0 << 0))

#define BV6(B5, B4, B3, B2, B1, B0)                                            \
  ((B5 << 5) | (B4 << 4) | (B3 << 3) | (B2 << 2) | (B1 << 1) | (B0 << 0))

struct reti {
  unsigned *code, *data;
  unsigned pc, acc, in1, in2;
};

struct shadow {
  bool *valid;
  size_t code, data;
};

int main(int argc, char **argv) {

  // Some trivial option handling here.

  if (argc != 3) {
    printf("usage: emureti <code> <data>\n");
    exit(0);
  }

  const char *code_path = argv[1];
  const char *data_path = argv[2];

  if (!file_exists(code_path))
    die("code file '%s' does not exist", code_path);
  if (!file_exists(data_path))
    die("data file '%s' does not exist", data_path);

  // Allocate ReTI state.

  struct reti reti;	// Architectual state.
  struct shadow shadow; // Shadow state of simulator.

  // Read code file.

  FILE *code_file = fopen(code_path, "r");
  if (!code_file)
    die("could not read code file '%s'", code_path);
  reti.code = malloc(CAPACITY * sizeof *reti.code);
  if (!reti.code)
    die("could not allocate code");
  shadow.code = 0;
  unsigned word;
  while (fread(&word, sizeof word, 1, code_file) == 1)
    if (shadow.code == CAPACITY)
      die("capacity of code area reached");
    else
      reti.code[shadow.code++] = word;
  fclose(code_file);

  // Read data file and set valid memory area.

  shadow.valid = calloc(CAPACITY, sizeof *shadow.valid);
  if (!shadow.valid)
    die("could not allocate valid");
  FILE *data_file = fopen(data_path, "r");
  if (!data_file)
    die("could not read data file '%s'", data_path);
  reti.data = malloc(CAPACITY * sizeof *reti.data);
  if (!reti.data)
    die("could not allocate data");
  shadow.data = 0;
  while (fread(&word, sizeof word, 1, data_file) == 1)
    if (shadow.data == CAPACITY)
      die("capacity of data area reached");
    else {
      shadow.valid[shadow.data] = true;
      reti.data[shadow.data] = word;
      shadow.data++;
    }
  fclose(data_file);

  // Simulate code on data.

  reti.pc = 0;

  while (reti.pc < shadow.code) {
    const unsigned pc = reti.pc;
    const unsigned I = reti.code[pc];
    const unsigned I31to30 = I >> 30;
    const unsigned I31to28 = I >> 28;
    const unsigned I31to26 = I >> 26;
    const unsigned I31to27 = I >> 28;
#if 0
    const unsigned I25to24 = (I >> 24) & 3;
    const unsigned I27to26 = (I >> 26) & 3;
#endif
    switch (I31to30) {
    case BV2(0, 1): // Load Instructions
      switch (I31to28) {
      case BV4(0, 1, 0, 0): // LOAD D i
	break;
      case BV4(0, 1, 0, 1): // LOADIN1 D i
	break;
      case BV4(0, 1, 1, 0): // LOADIN2 D i
	break;
      case BV4(0, 1, 1, 1): // LOADI D i
	break;
      }
      break;
    case BV2(1, 0): // Store Instructions
      switch (I31to28) {
      case BV4(1, 0, 0, 0): // STORE i
	break;
      case BV4(1, 0, 0, 1): // STOREIN1 i
	break;
      case BV4(1, 0, 1, 0): // STOREIN2 i
	break;
      case BV4(1, 0, 1, 1): // MOVE S D
	break;
      }
      break;
    case BV2(0, 0): // Compute Instructions
      switch (I31to26) {
      case BV6(0, 0, 0, 0, 1, 0): // SUBI D i
	break;
      case BV6(0, 0, 0, 0, 1, 1): // ADDI D i
	break;
      case BV6(0, 0, 0, 1, 0, 0): // OPLUSI D i
	break;
      case BV6(0, 0, 0, 1, 0, 1): // ORI D i
	break;
      case BV6(0, 0, 0, 1, 1, 0): // ANDI D i
	break;
      case BV6(0, 0, 1, 0, 1, 0): // SUB D i
	break;
      case BV6(0, 0, 1, 0, 1, 1): // ADD D i
	break;
      case BV6(0, 0, 1, 1, 0, 0): // OPLUS D i
	break;
      case BV6(0, 0, 1, 1, 0, 1): // OR D i
	break;
      case BV6(0, 0, 1, 1, 1, 0): // AND D i
	break;
      }
      break;
    case BV2(1, 1): // Jump Instructions
      switch (I31to27) {
      case BV5(1, 1, 0, 0, 0): // NOP
	break;
      case BV5(1, 1, 0, 0, 1): // JUMP> i
	break;
      case BV5(1, 1, 0, 1, 0): // JUMP= i
	break;
      case BV5(1, 1, 0, 1, 1): // JUMP>= i
	break;
      case BV5(1, 1, 1, 0, 0): // JUMP< i
	break;
      case BV5(1, 1, 1, 0, 1): // JUMP!= i
	break;
      case BV5(1, 1, 1, 1, 0): // JUMP<= i
	break;
      case BV5(1, 1, 1, 1, 1): // JUMP i
	break;
      }
      break;
    }
    if (reti.pc == pc)
      break;
  }

  for (size_t i = 0; i != shadow.data; i++)
    if (shadow.valid[i]) {
      printf("%08x ", (unsigned)i);
      const unsigned word = reti.data[i];
      for (unsigned i = 0, tmp = word; i != 4; i++, tmp >>= 8)
	printf(" %02x", tmp & 0xff);
      fputs("  ", stdout);
      for (unsigned i = 0, tmp = word; i != 4; i++, tmp >>= 8) {
	int ch = tmp & 0xff;
	printf("%c", isprint(ch) ? ch : '.');
      }
      printf("  %10u  %11d\n", word, (int) word);
    }

  free(shadow.valid);
  free(reti.data);
  free(reti.code);

  return 0;
}
