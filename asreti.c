#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

// We read a ReTI assembler file.

static size_t lineno = 1;
static int last_read_char;
static const char *assembler_path;
static bool close_assembler_file;
static FILE *assembler_file;

// And write binary encoded ReTI code file.

static const char *code_path;
static bool close_code_file;
static FILE *code_file;

// Two generic error functions.

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

// Four factored out short-cuts to common parse error.

static void invalid_instruction() { error("invalid instruction"); }

static void invalid_source() { error("invalid source register"); }

static void invalid_destination() { error("invalid destination register"); }

static void invalid_immediate() { error("invalid immediate"); }

// Check whether the given path points to a file.

static bool file_exists(const char *path) {
  struct stat buf;
  return !stat(path, &buf);
}

// Read from the assembler file, handle DOS/Windows carriage return and
// update line number counter 'lineno'.

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

// Allows compile time constants for bit-vectors (6-bit prefix of machine code).

#define CODE(A, B, C, D, E, F)                                                 \
  ((((unsigned)(A)) << 31) | (((unsigned)(B)) << 30) |                         \
   (((unsigned)(C)) << 29) | (((unsigned)(D)) << 28) |                         \
   (((unsigned)(E)) << 27) | (((unsigned)(F)) << 26))

// clang-format off

// This enumeration has all our 6-bit most-significant prefix code-words.

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

  // Command line option parsing.

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

  // Open and read assembler file.

  if (!assembler_path)
    assembler_path = "<stdin>", assembler_file = stdin;
  else if (!file_exists(assembler_path))
    die("can not find assembler file '%s'", assembler_path);
  else if (!(assembler_file = fopen(assembler_path, "r")))
    die("can not read assembler file '%s'", assembler_path);
  else
    close_assembler_file = true;

  // Open and write code file.

  if (!code_path && isatty(1))
    die("will not write binary code to terminal");
  else if (!code_path)
    code_path = "<stdout>", code_file = stdout;
  else if (!(code_file = fopen(code_path, "w")))
    die("can not write code file '%s'", code_path);
  else
    close_code_file = true;

  // This loops reads assembler instructions from the assembler file and
  // writes them in a single pass to the output code file.

  for (;;) {

    int ch = read_char();

    // These flags determine after parsing the name of the
    // instruction whether we need to read 'S', 'D' and 'i'.

    bool parse_source = false;     // Only for 'MOVE' necessary.
    bool parse_destination = true; // Most instructions require 'D'.
    bool parse_immediate = true;   // Most instructions require 'i'.

    // This word accumulates the machine code of the parsed instruction.

    unsigned code = NOP;

    switch (ch) {

    case ' ':
    case '\t':
      continue; // Skip white space at the beginning of the line.

    case EOF:
      goto DONE; // Terminate if end-of-file is reached.

      // Full line comments start with ';'.

    case ';':
      while ((ch = read_char()) != '\n')
        if (ch == EOF)
          error("unexpected end-of-file in comment");
      continue;

      // Two specific error situations.

    case '\n':
      error("unexpected empty line");
      break;

    default:
      if (isprint(ch))
        error("unexpected character '%c'", ch);
      else
        error("unexpected character code '0x%02x'", ch);
      break;

      // The remaining parsing is done alphabetically with respect to the
      // first character of the instruction read.

    case 'A':
      ch = read_char();
      if (ch == 'D') {
        ch = read_char();
        if (ch == 'D') {
          ch = read_char();
          if (ch == ' ')
            code = ADD; // D i
          else if (ch == 'I') {
            code = ADDI; // D i
            ch = read_char();
          } else
            invalid_instruction();
        } else
          invalid_instruction();
      } else if (ch == 'N') {
        ch = read_char();
        if (ch == 'D') {
          ch = read_char();
          if (ch == ' ')
            code = AND; // D i
          else if (ch == 'I') {
            code = ANDI; // D i
            ch = read_char();
          } else
            invalid_instruction();
        } else
          invalid_instruction();
      } else
        invalid_instruction();
      break;

    case 'J':
      for (const char *p = "UMP"; *p; p++)
        if (*p != read_char())
          invalid_instruction();
      ch = read_char();
      if (ch == ' ')
        code = JUMP; // i
      else if (ch == '>') {
        ch = read_char();
        if (ch == ' ')
          code = JUMPGT; // i
        else if (ch == '=') {
          code = JUMPGE; // i
          ch = read_char();
        } else
          invalid_instruction();
      } else if (ch == '=') {
        code = JUMPEQ; // i
        ch = read_char();
      } else if (ch == '<') {
        if (ch == ' ')
          code = JUMPLT; // i
        else if (ch == '=') {
          code = JUMPLE; // i
          ch = read_char();
        } else
          invalid_instruction();
      } else if (ch == '!') {
        ch = read_char();
        if (ch == '=') {
          code = JUMPNE; // i
          ch = read_char();
        } else
          invalid_instruction();
      } else
        invalid_instruction();
      parse_destination = false;
      break;

    case 'L':
      for (const char *p = "OAD"; *p; p++)
        if (*p != read_char())
          invalid_instruction();
      ch = read_char();
      if (ch == ' ')
        code = LOAD; // D i
      else if (ch == 'I') {
        ch = read_char();
        if (ch == ' ')
          code = LOADI; // D i
        else {
          ch = read_char();
          if (ch == 'N') {
            ch = read_char();
            if (ch == '1')
              code = LOADIN1; // D i
            else if (ch == '2')
              code = LOADIN2; // D i
            else
              invalid_instruction();
            ch = read_char();
          } else
            invalid_instruction();
        }
      } else
        invalid_instruction();
      break;

    case 'M':
      for (const char *p = "OVE"; *p; p++)
        if (*p != read_char())
          invalid_instruction();
      code = MOVE; // S D
      parse_source = true;
      parse_immediate = false;
      ch = read_char();
      break;

    case 'N':
      for (const char *p = "OP"; *p; p++)
        if (*p != read_char())
          invalid_instruction();
      code = NOP;
      ch = read_char();
      parse_destination = false;
      parse_immediate = false;
      break;

    case 'O':
      ch = read_char();
      if (ch == 'P') {
        for (const char *p = "LUS"; *p; p++)
          if (*p != read_char())
            invalid_instruction();
        ch = read_char();
        if (ch == ' ')
          code = OPLUS; // D i;
        else if (ch == 'I') {
          code = OPLUSI; // D i;
          ch = read_char();
        } else
          invalid_instruction();
      } else if (ch == 'R') {
        ch = read_char();
        if (ch == ' ')
          code = OR; // D i
        else if (ch == 'I') {
          code = ORI; // D i
          ch = read_char();
        } else
          invalid_instruction();
      } else
        invalid_instruction();
      break;

    case 'S':
      ch = read_char();
      if (ch == 'T') {
        parse_destination = false;
        for (const char *p = "ORE"; *p; p++)
          if (*p != read_char())
            invalid_instruction();
        ch = read_char();
        if (ch == ' ')
          code = STORE; // i
        else if (ch == 'I') {
          ch = read_char();
          if (ch == 'N') {
            ch = read_char();
            if (ch == '1')
              code = STOREIN1; // i
            else if (ch == '2')
              code = STOREIN2; // i
            else
              invalid_instruction();
            ch = read_char();
          } else
            invalid_instruction();
        } else
          invalid_instruction();
      } else if (ch == 'U') {
        ch = read_char();
        if (ch == 'B') {
          ch = read_char();
          if (ch == ' ')
            code = SUB; // D i;
          else if (ch == 'I') {
            code = SUBI; // D i;
            ch = read_char();
          } else
            invalid_instruction();
        } else
          invalid_instruction();
      } else
        invalid_instruction();
      break;
    }

    // After parsing the prefix the instruction and setting its code we
    // parse the remaining parts of an instruction ('S', 'D' and 'i').

    if (parse_source) { // Parse source register 'S'.
      if (ch != ' ')
        invalid_instruction();
      assert(code == MOVE);
      unsigned S = 0;
      ch = read_char();
      if (ch == 'A') {
        for (const char *p = "CC"; *p; p++)
          if (*p != read_char())
            invalid_source();
        S = 3;
      } else if (ch == 'I') {
        ch = read_char();
        if (ch != 'N')
          invalid_source();
        ch = read_char();
        if (ch == '1')
          S = 1;
        else if (ch == '2')
          S = 2;
        else
          invalid_source();
      } else if (ch == 'P') {
        if (read_char() != 'C')
          invalid_source();
        assert(!S);
      } else
        invalid_source();
      code |= S << 26;
      ch = read_char();
    }

    if (parse_destination) { // Parse destination register 'D'.
      if (ch != ' ') {
        if (parse_source)
          invalid_source();
        else
          invalid_instruction();
      }
      unsigned D = 0;
      ch = read_char();
      if (ch == 'A') {
        for (const char *p = "CC"; *p; p++)
          if (*p != read_char())
            invalid_destination();
        D = 3;
      } else if (ch == 'I') {
        ch = read_char();
        if (ch != 'N')
          invalid_destination();
        ch = read_char();
        if (ch == '1')
          D = 1;
        else if (ch == '2')
          D = 2;
        else
          invalid_destination();
      } else if (ch == 'P') {
        if (read_char() != 'C')
          invalid_destination();
        assert(!D);
      } else
        invalid_source();
      code |= D << 24;
      ch = read_char();
    }

    if (parse_immediate) { // Parse immediate 'i'.
      if (ch != ' ') {
        if (parse_destination)
          invalid_destination();
        else {
          assert(!parse_source);
          invalid_instruction();
        }
      }
      ch = read_char();
      unsigned i;
      if (ch == '-') {
        ch = read_char();
        if (ch == '0' || !isdigit(ch))
          invalid_immediate();
        i = (ch - '0');
        const unsigned max_immediate = 0x800000;
        while (isdigit(ch = read_char())) {
          if (max_immediate / 10 < i)
            invalid_immediate();
          i *= 10;
          int digit = ch - '0';
          if (max_immediate - digit < i)
            invalid_immediate();
          i += digit;
        }
        assert(i <= max_immediate);
        i = (~i + 1) & 0xffffff;
        code |= i;
      } else {
        if (!isdigit(ch))
          invalid_immediate();
        i = (ch - '0');
        const unsigned max_immediate = 0xffffff;
        while (isdigit(ch = read_char())) {
          if (max_immediate / 10 < i)
            invalid_immediate();
          i *= 10;
          int digit = ch - '0';
          if (max_immediate - digit < i)
            invalid_immediate();
          i += digit;
        }
      }
      assert(i <= 0xffffff);
      code |= i;
    }

    if (ch != ' ' && ch != '\t' && ch != ';' && ch != '\n') {
      if (parse_immediate)
        invalid_immediate();
      else if (parse_destination)
        invalid_destination();
      else
        invalid_source();
    }

    // Skip white space after a complete instruction.

    while (ch == ' ' || ch == '\t')
      ch = read_char();

    // Skip comments after an instruction.

    if (ch == ';') {
      while ((ch = read_char()) != '\n')
        if (ch == EOF)
          error("unexpected end-of-file in comment");
    }

    if (ch != '\n')
      error("expected new-line");

    // Write the word in little endian encoding to the code file.

    for (unsigned byte = 0; byte != 4; byte++) {
      const unsigned shift = byte * 8;
      unsigned char ch = code >> shift;
      fputc(ch, code_file);
    }
  }

DONE:

  if (close_assembler_file)
    fclose(assembler_file);

  if (close_code_file)
    fclose(code_file);

  return 0;
}
