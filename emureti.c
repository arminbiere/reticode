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
  size_t code_size;
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

  // Initialize ReTI state.

  struct reti reti;	// Architectual state.
  struct shadow shadow; // Shadow state of simulator.

  reti.pc = 0;

  // Read code file.

  FILE *code_file = fopen(code_path, "r");
  if (!code_file)
    die("could not read code file '%s'", code_path);
  reti.code = malloc(CAPACITY * sizeof *reti.code);
  if (!reti.code)
    die("could not allocate code");
  shadow.code_size = 0;
  unsigned word;
  while (fread(&word, sizeof word, 1, code_file) == 1)
    if (shadow.code_size == CAPACITY)
      die("capacity of code area reached");
    else
      reti.code[shadow.code_size++] = word;
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
  size_t data_size = 0;
  while (fread(&word, sizeof word, 1, data_file) == 1)
    if (data_size == CAPACITY)
      die("capacity of data area reached");
    else {
      shadow.valid[data_size] = true;
      reti.data[data_size] = word;
      data_size++;
    }
  fclose(data_file);

  while (reti.pc < shadow.code_size) {
    const unsigned I = reti.code[reti.pc];
    const unsigned I31to30 = I >> 30;
    const unsigned I31to28 = I >> 28;
    const unsigned I31to27 = I >> 28;
    const unsigned I25to24 = (I >> 24) & 3;
    const unsigned I27to26 = (I >> 26) & 3;
    switch (I31to30) {
    case BV2(0, 1):
      break;
    case BV2(1, 0):
      break;
    case BV2(0, 0):
      break;
    case BV2(1, 1):
      break;
    }
  }

  free(reti.code);
  free(reti.data);
  free(shadow.valid);

  return 0;
}
