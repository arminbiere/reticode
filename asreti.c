#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define CAPACITY ((size_t)1 << 32)

static void die(const char *fmt, ...) {
  fputs("asreti: error: ", stderr);
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
    printf("usage: asreti <assembler> <code>\n");
    exit(0);
  }

  const char *assembler_path = argv[1];
  const char *code_path = argv[2];

  if (!file_exists(code_path))
    die("assembler file '%s' does not exist", assembler_path);

  // Initialize code.

  unsigned * code = malloc (CAPACITY * sizeof *code);
  if (!code)
    die ("could not allocate code");

  // Read assembler file.

  FILE * assembler_file = fopen (assembler_path, "r");
  if (!assembler_file)
    die ("could not read assembler file '%s'", assembler_path);
  fclose (assembler_file);

  free (code);

  return 0;
}
