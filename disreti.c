// clang-format off

static const char * usage =
"usage: disreti [ -h | --help ] [ <code> [ <assembler> ] ]\n";

// clang-format on

#include "disreti.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static size_t bytes, words;
static const char *input_path;
static bool close_input_file;
static FILE *input_file;

static const char *output_path;
static bool close_output_file;
static FILE *output_file;

static void die(const char *, ...) __attribute__((format(printf, 1, 2)));

static void die(const char *fmt, ...) {
  fputs("disreti: error: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

static void error(const char *, ...) __attribute__((format(printf, 1, 2)));

static void error(const char *fmt, ...) {
  fprintf(stderr,
          "disreti: parse error: "
          "at byte %zu after %zu words in '%s': ",
          bytes, words, input_path);
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

static int read_char(void) {
  int ch = getc(input_file);
  if (ch != EOF)
    bytes++;
  return ch;
}

static bool read_word(unsigned *word_ptr) {
  int ch = read_char();
  if (ch == EOF)
    return false;
  unsigned word = (unsigned)(ch & 0xff);
  ch = read_char();
  if (ch == EOF)
    error("three bytes of word missing");
  word |= (unsigned)(ch & 0xff) << 8;
  ch = read_char();
  if (ch == EOF)
    error("two bytes of word missing");
  word |= (unsigned)(ch & 0xff) << 16;
  ch = read_char();
  if (ch == EOF)
    error("last byte of word missing");
  word |= (unsigned)(ch & 0xff) << 24;
  *word_ptr = word;
  words++;
  return true;
}

int main(int argc, char **argv) {
  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h") || !strcmp(arg, "--argv")) {
      fputs(usage, stdout);
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

  if (!input_path || !strcmp(input_path, "-"))
    input_path = "<stdin>", input_file = stdin;
  else if (!file_exists(input_path))
    die("could not find input file '%s'", input_path);
  else if (!(input_file = fopen(input_path, "r")))
    die("could not read input file '%s'", input_path);
  else
    close_input_file = true;

  if (!output_path || !strcmp(output_path, "-"))
    output_path = "<stdout>", output_file = stdout;
  else if (!(output_file = fopen(output_path, "w")))
    die("could not write output file '%s'", output_path);
  else
    close_output_file = true;

  unsigned code, disassembled = 0;
  char instruction[disassembled_reti_code_length];
  while (read_word(&code)) {
    if (!disassemble_reti_code(code, instruction))
      error("illegal instruction '0x%08x'", code);
    printf("%-21s ; %08x %08x\n", instruction, disassembled++, code);
  }

  if (close_input_file)
    fclose(input_file);
  if (close_output_file)
    fclose(output_file);

  return 0;
}
