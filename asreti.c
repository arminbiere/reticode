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

static char *line;
static size_t size_line;
static size_t capacity_line;

// And write binary encoded ReTI code file.

static const char *code_path;
static bool close_code_file;
static FILE *code_file;

// A generic error functions.

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

// Parse error function.

static bool non_empty_line(void) {
  for (size_t i = 0; i != size_line; i++) {
    const int ch = line[i];
    if (ch == ';')
      return false;
    if (ch != ' ' && ch != '\t')
      return true;
  }
  return false;
}

static bool is_end_of_line_character(int ch) {
  return ch == ';' || ch == '\n' || ch == EOF;
}

static bool is_symbol_character(int ch) {
  if ('A' <= ch && ch <= 'Z')
    return true;
  if ('a' <= ch && ch <= 'z')
    return true;
  if ('0' <= ch && ch <= '9')
    return true;
  if (ch == '-' || ch == '<' || ch == '>' || ch == '=' || ch == '!')
    return true;
  return false;
}

static bool is_parsable_character(int ch) {
  if (is_symbol_character(ch))
    return true;
  if (is_end_of_line_character(ch))
    return true;
  if (ch == ' ')
    return true;
  return false;
}

static void error(const char *, ...) __attribute__((format(printf, 1, 2)));

static void error(const char *fmt, ...) {
  fprintf(stderr, "asreti: parse error: at line %zu in '%s': ",
	  lineno - (last_read_char == '\n'), assembler_path);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  if (non_empty_line()) {
    fputs(" in \"", stderr);
    size_t i = 0;
    int ch;
    while (i != size_line && ((ch = line[i]) == ' ' || ch == '\t'))
      i++;
    while (i != size_line) {
      ch = line[i++];
      if (ch == '\t')
	fputs("<tab>", stderr);
      else if (ch == '\n')
	fputs("<new-line>", stderr);
      else if (ch == EOF)
	fputs("<end-of-file>", stderr);
      else if (isprint(ch))
	fputc(ch, stderr);
      else
	fprintf(stderr, "<0x%02x>", ch);
    }
    if (is_symbol_character(last_read_char))
      while (is_symbol_character(ch = getc(assembler_file)))
	putc(ch, stderr);
    fputc('"', stderr);
  }
  fputc('\n', stderr);
  exit(1);
}

// Four factored out short-cuts to common parse error.

static void invalid_instruction() {
  const int ch = last_read_char;
  if (is_symbol_character(ch))
    error("invalid instruction");
  else if (is_end_of_line_character(ch))
    error("expected space after instruction");
  else {
    assert(ch != ' ');
    assert(!is_parsable_character(ch));
    if (isprint(ch))
      error("invalid character '%c' in instruction", ch);
    else
      error("invalid character code '<0x%02x>' in instruction", ch);
  }
}

// Check whether the given path points to a file.

static bool file_exists(const char *path) {
  struct stat buf;
  return !stat(path, &buf);
}

// Save read lines to 'line' for better parse error diagnosis.

static void push_char(int ch) {
  if (size_line == capacity_line) {
    capacity_line = capacity_line ? 2 * capacity_line : 1;
    line = realloc(line, capacity_line);
    if (!line)
      die("out-of-memory enlarging line buffer");
  }
  line[size_line++] = ch;
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
  if (last_read_char == '\n')
    size_line = 0;
  if (res == '\n')
    lineno++;
  else
    push_char(res);
  last_read_char = res;
  return res;
}

// Allows compile time constants for bit-vectors (6-bit prefix of machine
// code).

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

static int hexdigit(int ch) {
  if ('0' <= ch && ch <= '9')
    return ch - '0';
  if ('a' <= ch && ch <= 'f')
    return 10 + (ch - 'a');
  if ('A' <= ch && ch <= 'F')
    return 10 + (ch - 'A');
  return -1;
}

// Parse either source or destination register.

static unsigned parse_register(const char *type) {
  unsigned code = 0;
  int ch = read_char();
  if (ch == 'A') {
    if (read_char() != 'C')
      error("expected 'C' after 'A'");
    if (read_char() != 'C')
      error("expected 'C' after \"AC\"");
    code = 3;
  } else if (ch == 'I') {
    ch = read_char();
    if (ch != 'N')
      error("expected 'N' after 'I'");
    ch = read_char();
    if (ch == '1')
      code = 1;
    else if (ch == '2')
      code = 2;
    else
      error("expected '1' or '2' after \"IN\"");
  } else if (ch == 'P') {
    if (read_char() != 'C')
      error("expected 'C' after 'P'");
    ch = read_char();
    if (ch != ' ')
      error("expected space after \"PC\"");
    assert(!code);
  } else if (ch == ' ')
    error("unexpected space instead of %s register", type);
  else if (is_end_of_line_character(ch))
    error("%s register missing", type);
  else if (is_parsable_character(ch), type)
    error("invalid %s register", type);
  else if (isprint(ch))
    error("invalid character '%c' expecting %s register", ch, type);
  else
    error("invalid character code '<0x%02x>' "
	  "expecting %s register",
	  ch, type);
  return code;
}

int main(int argc, char **argv) {

  // Command line option parsing.

  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
      printf("usage: asreti [ -h | --help ] <assembler> <code>\n");
      exit(0);
    } else if (arg[0] == '-' && arg[1])
      die("invalid option '%s' (try '-h')", arg);
    else if (!assembler_path)
      assembler_path = arg;
    else if (!code_path)
      code_path = arg;
  }

  // Open and read assembler file.

  if (assembler_path && !strcmp(assembler_path, "-"))
    assembler_path = 0;

  if (!assembler_path)
    assembler_path = "<stdin>", assembler_file = stdin;
  else if (!file_exists(assembler_path))
    die("can not find assembler file '%s'", assembler_path);
  else if (!(assembler_file = fopen(assembler_path, "r")))
    die("can not read assembler file '%s'", assembler_path);
  else
    close_assembler_file = true;

  // Open and write code file.

  if (code_path && !strcmp(code_path, "-"))
    code_path = 0;

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

    bool parse_source = false;	   // Only for 'MOVE' necessary.
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
      if (is_parsable_character(ch))
	error("unexpected character '%c'", ch);
      else if (isprint(ch))
	error("invalid character '%c'", ch);
      else
	error("invalid character code '0x%02x'", ch);
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
	ch = read_char();
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
	else if (ch == 'N') {
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

    if (ch != ' ')
      invalid_instruction();

    // After parsing the prefix the instruction and setting its code we
    // parse the remaining parts of an instruction ('S', 'D' and 'i').

    if (parse_source) {
      assert(code == MOVE);
      const unsigned S = parse_register("source");
      code |= S << 26;
      ch = read_char();
      if (ch != ' ') {
	if (is_parsable_character(ch))
	  error("invalid source register");
	else
	  error("expected space after source register");
      }
      assert(parse_destination);
    }

    if (parse_destination) {
      const unsigned D = parse_register("destination");
      code |= D << 24;
      ch = read_char();
      if (parse_immediate) {
	if (ch != ' ') {
	  if (is_parsable_character(ch))
	    error("invalid destination register");
	  else
	    error("expected space after destination register");
	}
      } else if (is_parsable_character(ch))
	error("invalid destination register");
    }

    if (parse_immediate) {
      ch = read_char();
      unsigned i;
      if (ch == ' ')
	error("unexpected space instead of immediate");
      else if (is_end_of_line_character(ch))
	error("immediate misssing");
      else if (ch == '-') {
	ch = read_char();
	if (ch == '0')
	  error("unexpected '0' after '-'");
	if (!isdigit(ch))
	  error("expected digit after '-'");
	i = (ch - '0');
	const unsigned max_immediate = 0x800000;
	ch = read_char();
	if (ch == 'x') {
	  int digit;
	  ch = read_char();
	  while ((digit = hexdigit(ch)) >= 0) {
	    if (max_immediate / 16 < i)
	      error("maximum negative immediate exceeded");
	    i *= 16;
	    if (max_immediate - digit < i)
	      error("maximum negative immediate exceeded");
	    i += digit;
	    ch = read_char();
	  }
	} else {
	  while (isdigit(ch)) {
	    if (max_immediate / 10 < i)
	      error("maximum negative immediate exceeded");
	    i *= 10;
	    int digit = ch - '0';
	    if (max_immediate - digit < i)
	      error("maximum negative immediate exceeded");
	    i += digit;
	    ch = read_char();
	  }
	}
	assert(i <= max_immediate);
	i = (~i + 1) & 0xffffff;
	code |= i;
      } else if (isdigit(ch)) {
	i = (ch - '0');
	const unsigned max_immediate = 0xffffff;
	ch = read_char();
	if (ch == 'x') {
	  int digit;
	  ch = read_char();
	  while ((digit = hexdigit(ch)) >= 0) {
	    if (max_immediate / 16 < i)
	      error("maximum immediate exceeded");
	    i *= 16;
	    if (max_immediate - digit < i)
	      error("maximum immediate exceeded");
	    i += digit;
	    ch = read_char();
	  }
	} else {
	  while (isdigit(ch)) {
	    if (max_immediate / 10 < i)
	      error("maximum immediate exceeded");
	    i *= 10;
	    int digit = ch - '0';
	    if (max_immediate - digit < i)
	      error("maximum immediate exceeded");
	    i += digit;
	    ch = read_char();
	  }
	}
      } else if (isprint(ch))
	error("unexpected character '%c' expecting immediate", ch);
      else
	error("unexpected character code '<0x%02x>' expecting immediate", ch);
      assert(i <= 0xffffff);
      code |= i;

      if (is_parsable_character(ch))
	error("invalid immediate");
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

  free(line);

  if (close_assembler_file)
    fclose(assembler_file);

  if (close_code_file)
    fclose(code_file);

  return 0;
}
