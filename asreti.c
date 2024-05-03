#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static size_t lineno = 1;
static int last_read_char;
static const char *assembler_path;
static FILE *assembler_file;

static const char *code_path;
static bool close_code_file;
static FILE *code_file;

static void die(const char *, ...) __attribute__((format(printf, 1, 2)));

static void die(const char *fmt, ...) {
  fputs("asreti: error: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

static void error(const char *, ...) __attribute__((format(printf, 1, 2)));

static void error(const char *fmt, ...) {
  fprintf(stderr, "asreti: parse error: at line %zu in '%s': ",
	  lineno - (last_read_char == '\n'), assembler_path);
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
  int res = getc(assembler_file);
  if (res == '\r') {
    res = getc(assembler_file);
    if (res != '\n')
      error("missing new-line after carriage-return");
  }
  if (res == '\n')
    lineno++;
  last_read_char = res;
  return res;
}

static void invalid_instruction() { error("invalid instruction"); }

#define CODE(A, B, C, D, E, F)                                                 \
  ((((unsigned)(A)) << 31) | (((unsigned)(B)) << 30) |                         \
   (((unsigned)(C)) << 29) | (((unsigned)(D)) << 28) |                         \
   (((unsigned)(E)) << 27) | (((unsigned)(F)) << 26))

// clang-format off

enum code {
  LOAD =     CODE(0,1,0,0,0,0),
  LOADIN1 =  CODE(0,1,0,1,0,0),
  LOADIN2 =  CODE(0,1,1,0,0,0),
  LOADI =    CODE(0,1,1,1,0,0),
  STORE =    CODE(1,0,0,0,0,0),
  STOREIN1 = CODE(1,0,0,1,0,0),
  STOREIN2 = CODE(1,0,1,0,0,0),
  MOVE =     CODE(1,0,1,1,0,0),
  SUBI =     CODE(0,0,0,0,1,0),
  ADDI =     CODE(0,0,0,0,1,1),
  OPLUSI =   CODE(0,0,0,1,0,0),
  ORI =      CODE(0,0,0,1,0,1),
  ANDI =     CODE(0,0,0,1,1,0),
  SUB =      CODE(0,0,1,0,1,0),
  ADD =      CODE(0,0,1,0,1,1),
  OPLUS =    CODE(0,0,1,1,0,0),
  OR =       CODE(0,0,1,1,0,1),
  AND =      CODE(0,0,1,1,1,0),
  NOP =      CODE(1,1,0,0,0,0),
  JUMPGT =   CODE(1,1,0,0,1,0),
  JUMPEQ =   CODE(1,1,0,1,0,0),
  JUMPGE =   CODE(1,1,0,1,1,0),
  JUMPLT =   CODE(1,1,1,0,0,0),
  JUMPNE =   CODE(1,1,1,0,1,0),
  JUMPLE =   CODE(1,1,1,1,0,0),
  JUMP =     CODE(1,1,1,1,1,0),
};

// clang-format on

int main(int argc, char **argv) {

  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
      printf("usage: asreti [ -h | --help ] <assembler> <code>\n");
      exit(0);
    } else if (arg[0] == '-')
      die("invalid option '%s' (try '-h')", arg);
    else if (!assembler_path)
      assembler_path = arg;
    else if (!code_path)
      code_path = arg;
  }

  if (!assembler_path)
    die("assembler file missing");
  if (!file_exists(assembler_path))
    die("could not find assembler file '%s'", assembler_path);
  if (!(assembler_file = fopen(assembler_path, "r")))
    die("could not read assembler file '%s'", assembler_path);

  if (!code_path)
    code_path = "<stdout>", code_file = stdout;
  else if (!(code_file = fopen(code_path, "w")))
    die("could not write code file '%s'", code_path);
  else
    close_code_file = true;

  for (;;) {
    int ch = read_char();
    bool parse_source = false;
    bool parse_register = true;
    bool parse_immediate = true;
    enum code code = NOP;
    switch (ch) {
    case ' ':
    case '\t':
      continue;
    case EOF:
      goto DONE;
    case ';':
      while ((ch = read_char()) != '\n')
	if (ch == EOF)
	  error("unexpected end-of-file in comment");
      continue;
    case 'L':
      for (const char *p = "OAD"; *p; p++)
	if (*p != read_char())
	  invalid_instruction();
      ch = read_char();
      if (ch == ' ')
	code = encode4(0, 1, 0, 0); // LOAD D i
      else if (ch == 'I') {
	ch = read_char();
	if (ch == ' ')
	  code = encode4(0, 1, 1, 1); // LOADI D i
	else {
	  ch = read_char();
	  if (ch == 'N') {
	    ch = read_char();
	    if (ch == '1')
	      code = encode4(0, 1, 0, 1); // LOADIN1 D i
	    else if (ch == '2')
	      code = encode4(0, 1, 1, 0); // LOADIN2 D i
	    else
	      invalid_instruction();
	    ch = read_char();
	  } else
	    invalid_instruction();
	}
      } else
	invalid_instruction();
      break;
    case 'S':
      parse_register = false;
      for (const char *p = "TORE"; *p; p++)
	if (*p != read_char())
	  invalid_instruction();
      ch = read_char();
      if (ch == ' ')
	code = encode4(1, 0, 0, 0); // STORE i
      else if (ch == 'I') {
	ch = read_char();
	if (ch == 'N') {
	  ch = read_char();
	  if (ch == '1')
	    code = encode4(1, 0, 0, 1); // STOREIN1 i
	  else if (ch == '2')
	    code = encode4(1, 0, 1, 0); // STOREIN2 i
	  else
	    invalid_instruction();
	  ch = read_char();
	} else
	  invalid_instruction();
      } else
	invalid_instruction();
      break;
    case 'M':
      for (const char *p = "OVE"; *p; p++)
	if (*p != read_char())
	  invalid_instruction();
      code = encode4 (
      ch = read_char ();
      break;
    case '\n':
      error("unexpected empty line");
      break;
    default:
      if (isprint(ch))
	error("unexpected character '%c'", ch);
      else
	error("unexpected character code 0x%02x", ch);
      break;
    }
    if (ch != ' ')
      invalid_instruction();
  }

DONE:
  fclose(assembler_file);
  if (close_code_file)
    fclose(code_file);

  return 0;
}
