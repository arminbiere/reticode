#include <limits.h>
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
static const char *input_path;
static bool close_input_file;
static FILE *input_file;

static FILE *output_file;
static const char *output_path;
static bool close_output_file;

// Two generic error functions.

static void die(const char *, ...) __attribute__((format(printf, 1, 2)));

static void die(const char *fmt, ...) {
  fflush (stdout);
  fputs("enchex: error: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

static void error(const char *, ...) __attribute__((format(printf, 1, 2)));

static void error(const char *fmt, ...) {
  fflush (stdout);
  fprintf(stderr, "enchex: parse error: at line %zu in '%s': ",
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

static int char2hex(int ch) {
  if ('0' <= ch && ch <= '9')
    return ch - '0';
  if ('a' <= ch && ch <= 'f')
    return 10 + (ch - 'a');
  if ('A' <= ch && ch <= 'F')
    return 10 + (ch - 'A');
  return EOF;
}

int main(int argc, char **argv) {

  // Option parsing.

  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
      printf("usage: enchex [ <input> [ <output> ] ]\n");
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

  if (!output_path && isatty(1))
    die("will not write binary data to terminal");
  else if (!output_path)
    output_path = "<stdout>", output_file = stdout;
  else if (!(output_file = fopen(output_path, "w")))
    die("could not write output file '%s'", output_path);
  else
    close_output_file = true;

  // Parse input and write output.

  size_t words = 0;

  for (;;) {
    int ch = read_char();
    if (ch == EOF)
      break;
    if (ch == '\n')
      error("invalid empty line");
    if (ch == ';') {
      while ((ch = read_char()) != '\n')
        if (ch == EOF)
          error("unexpected end-of-file in comment");
      continue;
    }
    unsigned address = 0;
    for (unsigned nibble = 0; nibble != 8; nibble++) {
      int digit = char2hex(ch);
      if (digit < 0)
        error("invalid address");
      address <<= 4;
      address |= digit;
      ch = read_char();
    }
    if (ch != ' ')
      error("expected space after address");
    ch = read_char();
    if (words > address)
      error("address 0x%08x below parsed words 0x%08x", address,
            (unsigned)(words - 1));
    while (words < address) {
      for (unsigned byte = 0; byte != 4; byte++)
        fputc((unsigned char)0, output_file);
      words++;
    }
    unsigned data = 0;
    for (unsigned nibble = 0; nibble != 8; nibble++) {
      int digit = char2hex(ch);
      if (digit < 0)
        error("invalid data");
      data <<= 4;
      data |= digit;
      ch = read_char();
    }
    if (ch != ' ' && ch != '\t' && ch != ';' && ch != '\n')
      error("expected white-space after data");

    if (words > UINT_MAX)
      error("maximum data capacity exhausted");

    // Skip white space after data.

    while (ch == ' ' || ch == '\t')
      ch = read_char();

    // Skip comments after data.

    if (ch == ';') {
      while ((ch = read_char()) != '\n')
        if (ch == EOF)
          error("unexpected end-of-file in comment");
    }

    if (ch != '\n')
      error("expected new-line");

    // Write the data word in little endian encoding to the output file.

    for (unsigned byte = 0; byte != 4; byte++) {
      const unsigned shift = byte * 8;
      unsigned char ch = data >> shift;
      fputc(ch, output_file);
    }

    words++;
  }

  if (close_input_file)
    fclose(input_file);
  if (close_output_file)
    fclose(output_file);

  return 0;
}
