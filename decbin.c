#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static size_t byteno;
static size_t wordno;
static const char *input_path;
static bool close_input_file;
static FILE *input_file;

static const char *output_path;
static bool close_output_file;
static FILE *output_file;

// Two generic error functions.

static void die(const char *, ...) __attribute__((format(printf, 1, 2)));

static void die(const char *fmt, ...) {
  fputs("decbin: error: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

static void error(const char *, ...) __attribute__((format(printf, 1, 2)));

static void error(const char *fmt, ...) {
  fprintf(stderr, "decbin: parse error: at word %zu byte %zu in '%s': ", wordno,
          byteno, input_path);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

// Check whether the given path points to a file.

static bool file_exists(const char *path) {
  struct stat buf;
  return !stat(path, &buf);
}

// Parsing bytes and words.

static bool read_byte(unsigned char *byte_ptr) {
  const int res = getc(input_file);
  if (res == EOF)
    return false;
  const unsigned char byte = res & 0xff;
  *byte_ptr = byte;
  byteno++;
  return true;
}

static bool read_word(unsigned *word_ptr) {
  unsigned char byte[4];
  if (!read_byte(byte + 0))
    return false;
  for (unsigned i = 1; i != 4; i++)
    if (!read_byte(byte + i))
      error("incomplete word");
  unsigned word = (byte[3] << 24) | (byte[2] << 16) | (byte[1] << 8) | byte[0];
  *word_ptr = word;
  wordno++;
  return true;
}

int main(int argc, char **argv) {
  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
      printf("usage: decbin [ <input> [ <output> ] ]\n");
      exit(0);
    } else if (arg[0] == '-' && arg[1])
      die("invalid option '%s' (try '-h')", arg);
    else if (!input_path)
      input_path = arg;
    else if (!output_path)
      output_path = arg;
    else
      die("too many files '%s', '%s' and '%s' (try '-h')", input_path,
          output_path, arg);
  }

  // Open and read input file.

  if (input_path && !strcmp(input_path, "-"))
    input_path = 0;

  if (!input_path)
    input_path = "<stdin>", input_file = stdin;
  else if (!file_exists(input_path))
    die("could not find input file '%s'", input_path);
  else if (!(input_file = fopen(input_path, "r")))
    die("could not read input file '%s'", input_path);
  else
    close_input_file = true;

  // Open and write output file.

  if (output_path && !strcmp(output_path, "-"))
    output_path = 0;

  if (!output_path)
    output_path = "<stdout>", output_file = stdout;
  else if (!(output_file = fopen(output_path, "w")))
    die("could not write output file '%s'", output_path);
  else
    close_output_file = true;

  size_t words = 0;
  unsigned word;
  while (read_word(&word)) {
    if (words > UINT_MAX)
      error("too many words");
    printf("%08x %08x\n", (unsigned)words++, word);
  }

  if (close_input_file)
    fclose(input_file);
  if (close_output_file)
    fclose(output_file);

  return 0;
}
