#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static size_t lineno = 1;
static char last_input_char;
static char *input_path;
static FILE *input_file;
static bool close_input_file;

static char *output_path;
static FILE *output_file;
static bool close_output_file;

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
  fprintf(stderr, "decbin: parse error: at line %zu in '%s': ",
          lineno - (last_input_char == '\n'), input_path);
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

// Read from the input file, handle DOS/Windows carriage return and
// update line number counter 'lineno'.

static int read_char(void) {
  int res = getc(input_file);
  if (res == '\r') {
    res = getc(input_file);
    if (res != '\n')
      error("missing new-line after carriage-return");
  }
  if (res == '\n')
    lineno++;
  last_input_char = res;
  return res;
}

int main(int argc, char **argv) {
  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
      printf("usage: decbin [ <input> [ <output> ] ]\n");
      exit(0);
    }
  }
  return 0;
}
