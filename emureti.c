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

int main(int argc, char **argv) {

  // Some trivial option handling here.

  if (argc != 3) {
    printf("usage: emureti <binary> <data>\n");
    exit(0);
  }

  const char *binary_path = argv[1];
  const char *data_path = argv[2];

  if (!file_exists(binary_path))
    die("binary file '%s' does not exist", binary_path);
  if (!file_exists(data_path))
    die("data file '%s' does not exist", data_path);

  // Read binary file.

  FILE *binary_file = fopen(binary_path, "r");
  if (!binary_file)
    die("could not read binary file '%s'", binary_path);
  unsigned *binary = malloc(CAPACITY * sizeof *binary);
  if (!binary)
    die("could not allocate binary");
  size_t binary_size = 0;
  unsigned word;
  while (fread(&word, sizeof word, 1, binary_file) == 1)
    if (binary_size == CAPACITY)
      die("capacity of binary area reached");
    else
      binary[binary_size++] = word;
  fclose(binary_file);

  // Read data file and set valid memory area.

  bool *valid = calloc(CAPACITY, sizeof *valid);
  FILE *data_file = fopen(data_path, "r");
  if (!data_file)
    die("could not read data file '%s'", data_path);
  unsigned *data = malloc(CAPACITY * sizeof *data);
  if (!data)
    die("could not allocate data");
  size_t data_size = 0;
  while (fread(&word, sizeof word, 1, data_file) == 1)
    if (data_size == CAPACITY)
      die("capacity of data area reached");
    else {
      valid[data_size] = true;
      data[data_size] = word;
      data_size++;
    }
  fclose(data_file);

  unsigned pc = 0;
  unsigned acc, in1, in2;

  for (;;) {
    unsigned instruction = binary[pc];
  }

  free (binary);
  free (data);

  return 0;
}
