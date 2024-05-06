//----------------------------------------------------------------------------//

// clang-format off

static const char *usage = 
"usage: emreti [ -h | --help"
#ifndef NSTEPPING
" | -s | --step"
#endif
" ] [ <steps> ] [ <code> [ <data> ] ] \n"
"\n"
"with the following options:\n"
"\n"
"  -h | --help   print this command line option summary\n"
"  -g | --debug  stop on unitialized data memory access\n"
"  -i | --ignore no warning on unitialized data memory access\n"
#ifndef NSTEPPING
"  -s | --step   step through and print each instruction\n"
#endif
"\n"
"The '<code>' is a program in ReTI machine code and '<data>' some binary\n"
"data which is loaded as data memory initially. If '<code>' is missing\n"
"the program is read from '<stdin>' and if '<data>' is missing the data\n"
"memory is kept completely uninitialized.  All unitialized words of the\n"
"data memory are set to zero. Alternatively it is also possibly to use as\n"
"file name '-' to force reading from '<stdin>' (but only for one file).\n"
"\n"
"If program execution succeeds the final data memory is printed for all\n"
"data words that have been initialized either through reading '<data>'\n"
"initially or have benn written to during the execution of the program.\n"
"\n"
"If the number of limit is given the program stops after that\n"
"many instructions have been executed.  Otherwise it stops if either\n"
"an uninitialized instruction is reached above the program code or an\n"
"instruction which loops on itself (including illegal limit).\n"
;

// clang-format on

//----------------------------------------------------------------------------//

#include <assert.h>  // assert
#include <ctype.h>   // isdigit
#include <stdarg.h>  // va_list va_begin vfprintf va_end
#include <stdbool.h> // bool
#include <stdio.h>   // printf snprintf fputs fputc fflush fopen fclose
#include <stdlib.h>  // calloc free exit
#include <string.h>  // strcmp

//----------------------------------------------------------------------------//

#include <sys/stat.h>  // stat
#include <sys/types.h> // stat
#include <unistd.h>    // stat

/*------------------------------------------------------------------------*/

#ifndef NSTEPPING

#include "disreti.h"

#endif

//----------------------------------------------------------------------------//

// On Linux allocating 2^32 unsigned words for the code and data memories
// succeeds as it actually only allocates virtual memory, which is mapped
// (really allocated) only when used.  On other platforms you might want to
// set the capacity closer to the actually needed memory.

// Yields 2^32 words = 16 GB for each the code and data memory.
//
#define CAPACITY ((size_t)1 << 32)

// Yields 2^16 words = 256 KB for each the code and data memory.
//
// #define CAPACITY ((size_t)1 << 32) // yields 2^16 words = 256 KB

//----------------------------------------------------------------------------//

// These 'BV' macros allow to generate constant bit-vectors of the given
// size at compile time.  Using functions would not work.

#define BV2(B1, B0) ((B1 << 1) | (B0 << 0))

#define BV4(B3, B2, B1, B0) ((B3 << 3) | (B2 << 2) | (B1 << 1) | (B0 << 0))

#define BV5(B4, B3, B2, B1, B0)                                                \
  ((B4 << 4) | (B3 << 3) | (B2 << 2) | (B1 << 1) | (B0 << 0))

#define BV6(B5, B4, B3, B2, B1, B0)                                            \
  ((B5 << 5) | (B4 << 4) | (B3 << 3) | (B2 << 2) | (B1 << 1) | (B0 << 0))

//----------------------------------------------------------------------------//

// Exit with error message with 'printf' style usage.
//
// The following declaration lets the compiler produce error messages if the
// arguments do not match format, i.e., if a string 'const char*' is given while
// '%d' is specified for the corresponding argument.

static void die(const char *, ...) __attribute__((format(printf, 1, 2)));

static void die(const char *fmt, ...) {
  fflush (stdout);
  fputs("emreti: error: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

// Similarly for warnings (without exit though).

static void warn(const char *, ...) __attribute__((format(printf, 1, 2)));

static void warn(const char *fmt, ...) {
  fflush (stdout);
  fputs("emreti: warning: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  fflush(stderr);
}

//----------------------------------------------------------------------------//

// Check if the given path exists as a file.

static bool file_exists(const char *path) {
  struct stat buf;
  return !stat(path, &buf);
}

// Check if the string only contains digits (is a positive number).

static bool is_number_string(const char *str) {
  if (!isdigit(*str))
    return false;
  for (const char *p = str + 1; *p; p++)
    if (!isdigit(*p))
      return false;
  return true;
}

//----------------------------------------------------------------------------//

// The whole emulator runs in the main function.

int main(int argc, char **argv) {

  //--------------------------------------------------------------------------//
  // First parse command line options.

#ifndef NSTEPPING
  bool step = false;
#endif
  size_t steps = 0;

  int debug = 0; //-1=ignore, 0=warning, 1=abort on unitialized data access.

  const char *code_path = 0;
  const char *data_path = 0;
  const char *limit_string = 0;

  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
      fputs(usage, stdout);
      exit(0);
    } else if (!strcmp(arg, "-s") || !strcmp(arg, "--step")) {
#ifndef NSTEPPING
      step = true;
#else
      die("invalid option '%s' "
          "(configured and compiled without stepping support)",
          arg);
#endif
    } else if (!strcmp(arg, "-g") || !strcmp(arg, "--debug"))
      debug = 1;
    else if (!strcmp(arg, "-i") || !strcmp(arg, "--ignore"))
      debug = -1;
    else if (arg[0] == '-' && arg[1])
      die("invalid option '%s' (try '-h')", arg);
    else if (is_number_string(arg)) {
      if (limit_string)
        die("two steps limits '%s' and '%s'", limit_string, arg);
      if (file_exists(arg))
        die("steps limit '%s' matches file '%s'", arg, arg);
      limit_string = arg;
    } else if (!code_path)
      code_path = arg;
    else if (!data_path)
      data_path = arg;
    else
      die("more than two files specified '%s', '%s' and '%s' (try '-h')",
          code_path, data_path, arg);
  }

  const size_t max_limit = ~(size_t)0;
  size_t limit = max_limit;
  if (limit_string) {
    limit = 0;
    const char *p = limit_string;
    int ch;
    while ((ch = *p++)) {
      assert(isdigit(ch));
      if (max_limit / 10 < limit)
        die("maximum steps limit exceeded in '%s'", limit_string);
      limit *= 10;
      int digit = ch - '0';
      if (max_limit - digit < limit)
        die("maximum steps limit exceeded in '%s'", limit_string);
      limit += digit;
    }
  }

  if (code_path && data_path)
    if (!strcmp(code_path, "-") && !strcmp(data_path, "-"))
      die("can not read both code and data from '<stdin>'");

  //--------------------------------------------------------------------------//

  // The actual state of our ReTI machine is saved in this 'reti' structure.
  //
  // We can assume that 'unsigned' is a 32-bit word and thus we use 'unsigned'
  // whenever we refer to a register, data or machine word of ReTI.
  //
  struct {
    unsigned *code, *data;
    unsigned PC, ACC, IN1, IN2;
  } reti;

  reti.PC = reti.ACC = reti.IN1 = reti.IN2 = 0;

  // The shadow state determines valid (used) code and data ranges.

  struct {
    bool *valid;
    size_t code, data;
  } shadow;

  //--------------------------------------------------------------------------//

  // Allocate code, data and valid memory.

  reti.code = calloc(CAPACITY, sizeof *reti.code);
  if (!reti.code)
    die("can not allocate code");
  shadow.code = 0;

  reti.data = calloc(CAPACITY, sizeof *reti.data);
  if (!reti.data)
    die("can not allocate data");
  shadow.data = 0;

  shadow.valid = calloc(CAPACITY, sizeof *shadow.valid);
  if (!shadow.valid)
    die("can not allocate valid bit-map");

    // Read code file.

#ifndef NSTEPPING
  char instruction_format[16];
#endif

  {
    FILE *code_file = 0;
    bool close_code_file = false;
    if (!code_path || !strcmp(code_path, "-"))
      code_path = "<stdin>", code_file = stdin;
    else if (!file_exists(code_path))
      die("code file '%s' does not exist", code_path);
    else if (!(code_file = fopen(code_path, "r")))
      die("can not read code file '%s'", code_path);
    else
      close_code_file = true;
#ifndef NSTEPPING
    char instruction[32];
    size_t instruction_length = 0;
#endif
    unsigned code;
    while (fread(&code, sizeof code, 1, code_file) == 1) {
      if (shadow.code == CAPACITY)
        die("capacity of code area reached");
      else
        reti.code[shadow.code++] = code;
#ifndef NSTEPPING
      if (disassemble_reti_code(code, instruction)) {
        size_t length = strlen(instruction);
        if (length > instruction_length)
          instruction_length = length;
      }
#endif
    }
    if (close_code_file)
      fclose(code_file);
#ifndef NSTEPPING
    sprintf(instruction_format, "%%-%zus", instruction_length);
#endif
  }

  // Read data file.

  if (data_path) {
    FILE *data_file = 0;
    bool close_data_file = false;
    if (!strcmp(data_path, "-"))
      data_path = "<stdin>", data_file = stdin;
    else if (!file_exists(data_path))
      die("data file '%s' does not exist", data_path);
    else if (!(data_file = fopen(data_path, "r")))
      die("can not read data file '%s'", data_path);
    unsigned word;
    while (fread(&word, sizeof word, 1, data_file) == 1)
      if (shadow.data == CAPACITY)
        die("capacity of data area reached");
      else {
        shadow.valid[shadow.data] = true;
        reti.data[shadow.data] = word;
        shadow.data++;
      }
    if (close_data_file)
      fclose(data_file);
  }

  //--------------------------------------------------------------------------//

  // Simulate code on data.

#ifndef NSTEPPING // If the compile time flag 'STEPPING' is set (default).

  // Buffers for printing step information.

  // e.g., "SUBI ACC 0x123456"
  //
  char instruction[32];

  // e.g., "ACC = ACC - [0x123456] = 1193047 - 1193046 = 1 = [0x00000001]"
  //
  char action[128];

#define INSTRUCTION(...)                                                       \
  do {                                                                         \
    if (step)                                                                  \
      snprintf(instruction, 32, __VA_ARGS__);                                  \
  } while (0)

#define ACTION(...)                                                            \
  do {                                                                         \
    if (step)                                                                  \
      snprintf(action, 128, __VA_ARGS__);                                      \
  } while (0)

#else // If compile time flag 'STEPPING' is not set ignore step code.

#define INSTRUCTION(FMT, ...) /**/
#define ACTION(FMT, ...)      /**/

#endif

  //==========================================================================//

  // Run the emulation until we get to a self-loop or reach undefined code.

  for (;;) {

    if (steps++ == limit) {
      warn("steps limit '%zu' reached", limit);
      break;
    }

    const unsigned PC = reti.PC;
    const unsigned IN1 = reti.IN1;
    const unsigned IN2 = reti.IN2;
    const unsigned ACC = reti.ACC;

    if (PC >= shadow.code) {
#ifndef NSTEPPING
      if (step) {
        if (steps == 1)
          fputs("STEPS    PC       CODE     IN1      IN2      ACC\n", stdout);
        printf("%-8zu %08x ........ %08x %08x %08x <undefined>\n", steps, PC,
               IN1, IN2, ACC);
      }
#endif
      if (PC != shadow.code)
        warn("stopping at undefined 'code[0x%08x]' above 0x%08x", PC,
             (unsigned)(shadow.code - 1));
      break;
    }
    const unsigned I = reti.code[PC];

    const unsigned I31to30 = I >> 30;
    const unsigned I31to28 = I >> 28;
    const unsigned I31to27 = I >> 27;
    const unsigned I31to26 = I >> 26;
    const unsigned I27to26 = (I >> 26) & 3;
    const unsigned I25to24 = (I >> 24) & 3;
    const unsigned I23toI0 = I & 0xffffff;

    const unsigned i = I23toI0;
    const unsigned unsigned_immediate = i;
    const unsigned immediate_sign_bit = (i >> 23) & 1;
    const unsigned immediate_extension = immediate_sign_bit ? 0xff000000 : 0;
    const unsigned signed_immediate = immediate_extension | unsigned_immediate;

#ifndef NSTEPPING
    const int immediate_sign_char = immediate_sign_bit ? '-' : '+';
    const int abs_immediate = abs((int)signed_immediate);
#endif

    unsigned S = 0;
    const char *S_symbol = 0;

    switch (I27to26) {
    case BV2(0, 0):
      S = PC;
      S_symbol = "PC";
      break;
    case BV2(0, 1):
      S = IN1;
      S_symbol = "IN1";
      break;
    case BV2(1, 0):
      S = IN2;
      S_symbol = "IN2";
      break;
    case BV2(1, 1):
      S = ACC;
      S_symbol = "ACC";
      break;
    }

    // Determine pointer address of destination register (in any case).

    unsigned *D_pointer = 0;
    const char *D_symbol = 0;

    switch (I25to24) {
    case BV2(0, 0):
      D_pointer = &reti.PC;
      D_symbol = "PC";
      break;
    case BV2(0, 1):
      D_pointer = &reti.IN1;
      D_symbol = "IN1";
      break;
    case BV2(1, 0):
      D_pointer = &reti.IN2;
      D_symbol = "IN2";
      break;
    case BV2(1, 1):
      D_pointer = &reti.ACC;
      D_symbol = "ACC";
      break;
    }

#ifdef NSTEPPING
    (void)S_symbol; // To avoid compiler warning not using 'S_symbol'.
    (void)D_symbol; // To avoid compiler warning not using 'D_symbol'.
#endif

    unsigned PC_next = PC + 1; // Default is to increase PC.
    bool D_write = false;      // Default is not to write to register D.
    bool M_write = false;      // Default is not to write to memory.
    bool M_read = false;       // Default is not to read from memory.
    unsigned result = 0;       // Computed, loaded, or stored result.
    unsigned address = 0;      // Address to read from or write to memory.
    unsigned loaded;           // Loaded from memory.
    bool taken = false;
    char *comparison = 0;

    unsigned *M = reti.data; // Also used couple of times.

#ifndef NSTEPPING

    // Just make sure to have a valid string (with terminating zero).

    instruction[0] = action[0] = 0;

#endif

    // Now we decode the actual instruction and execute it.

    switch (I31to30) {

    case BV2(0, 1): // Load Instructions
      switch (I31to28) {
      case BV4(0, 1, 0, 0): // LOAD D i
        address = unsigned_immediate;
        result = M[address];
        INSTRUCTION("LOAD %s %u", D_symbol, i);
        ACTION("%s = M(<0x%x>) = M(0x%x) = 0x%x", D_symbol, i, address, result);
        M_read = true;
        D_write = true;
        break;
      case BV4(0, 1, 0, 1): // LOADIN1 D i
        address = IN1 + unsigned_immediate;
        INSTRUCTION("LOADIN1 %s %u", D_symbol, i);
        ACTION("%s = M(<IN1> + <0x%x>) = M(0x%x + 0x%x) = M(0x%x) = 0x%x",
               D_symbol, i, IN1, i, address, result);
        result = M[address];
        M_read = true;
        D_write = true;
        break;
      case BV4(0, 1, 1, 0): // LOADIN2 D i
        address = IN2 + unsigned_immediate;
        INSTRUCTION("LOADIN2 %s %u", D_symbol, i);
        ACTION("%s = M(<IN2> + <0x%x>) = M(0x%x + 0x%x) = M(0x%x) = 0x%x",
               D_symbol, i, IN2, i, address, result);
        result = M[address];
        M_read = true;
        D_write = true;
        break;
      case BV4(0, 1, 1, 1): // LOADI D i
        result = unsigned_immediate;
        INSTRUCTION("LOADI %s %u", D_symbol, i);
        ACTION("%s = 0x%x", D_symbol, i);
        D_write = true;
        break;
      }
      break; // end of Load Instructions

    case BV2(1, 0): // Store Instructions
      switch (I31to28) {
      case BV4(1, 0, 0, 0): // STORE i
        address = unsigned_immediate;
        result = ACC;
        INSTRUCTION("STORE %u", i);
        ACTION("M(<%u>) = M(0x%x) = 0x%x", i, address, result);
        M_write = true;
        break;
      case BV4(1, 0, 0, 1): // STOREIN1 i
        address = IN1 + unsigned_immediate;
        result = ACC;
        INSTRUCTION("STOREIN1 %u", i);
        ACTION("M(0x%x) = M(<IN1> + <0x%x>) = M(0x%x + 0x%x) = ACC = %x",
               address, i, IN1, i, result);
        M_write = true;
        break;
      case BV4(1, 0, 1, 0): // STOREIN2 i
        address = IN2 + unsigned_immediate;
        result = ACC;
        INSTRUCTION("STOREIN2 %u", i);
        ACTION("M(0x%x) = M(<IN2> + <0x%x>) = M(0x%x + 0x%x) = ACC = %x",
               address, i, IN2, i, result);
        M_write = true;
        break;
      case BV4(1, 0, 1, 1): // MOVE S D
        result = S;
        INSTRUCTION("MOVE %s %s", S_symbol, D_symbol);
        ACTION("%s = %s = 0x%x", D_symbol, S_symbol, result);
        D_write = true;
        break;
      }
      break; // end of Store Instructions

    case BV2(0, 0): // Compute Instructions
      unsigned D = *D_pointer;
      switch (I31to26) {
      case BV6(0, 0, 0, 0, 1, 0): // SUBI D i
        result = D - signed_immediate;
        INSTRUCTION("SUBI %s %d", D_symbol, signed_immediate);
        ACTION("%s = %s - [0x%x] = %d - %d = %d = [0x%x]", D_symbol, D_symbol,
               i, (int)D, (int)i, (int)result, result);
        D_write = true;
        break;
      case BV6(0, 0, 0, 0, 1, 1): // ADDI D i
        result = D + signed_immediate;
        INSTRUCTION("ADDI %s %d", D_symbol, signed_immediate);
        ACTION("%s = %s + [0x%x] = %d + %d = %d = [0x%x]", D_symbol, D_symbol,
               i, (int)D, (int)i, (int)result, result);
        D_write = true;
        break;
      case BV6(0, 0, 0, 1, 0, 0): // OPLUSI D i
        result = D ^ unsigned_immediate;
        INSTRUCTION("OPLUSI %s 0x%x", D_symbol, i);
        ACTION("%s = %s ^ 0x%x = 0x%x ^ 0x%x = 0x%x", D_symbol, D_symbol,
               unsigned_immediate, D, unsigned_immediate, result);
        D_write = true;
        break;
      case BV6(0, 0, 0, 1, 0, 1): // ORI D i
        result = D | unsigned_immediate;
        INSTRUCTION("ORI %s 0x%x", D_symbol, i);
        ACTION("%s = %s | 0x%x = 0x%x | 0x%x = 0x%x", D_symbol, D_symbol,
               unsigned_immediate, D, unsigned_immediate, result);
        D_write = true;
        break;
      case BV6(0, 0, 0, 1, 1, 0): // ANDI D i
        result = D & unsigned_immediate;
        INSTRUCTION("ANDI %s 0x%x", D_symbol, i);
        ACTION("%s = %s & 0x%x = 0x%x & 0x%x = 0x%x", D_symbol, D_symbol,
               unsigned_immediate, D, unsigned_immediate, result);
        D_write = true;
        break;
      case BV6(0, 0, 1, 0, 1, 0): // SUB D i
        address = unsigned_immediate;
        loaded = M[address];
        result = D - loaded;
        INSTRUCTION("SUB %s %d", D_symbol, signed_immediate);
        ACTION("%s = %s - M(<0x%x>) = %s - [0x%x] = %d - %d = %d = [0x%x]",
               D_symbol, D_symbol, i, D_symbol, loaded, (int)D, (int)loaded,
               (int)result, result);
        D_write = true;
        M_read = true;
        break;
      case BV6(0, 0, 1, 0, 1, 1): // ADD D i
        address = unsigned_immediate;
        loaded = M[address];
        result = D + loaded;
        INSTRUCTION("ADD %s %d", D_symbol, signed_immediate);
        ACTION("%s = %s + M(<0x%x>) = %s + [0x%x] = %d + %d = %d = [0x%x]",
               D_symbol, D_symbol, i, D_symbol, loaded, (int)D, (int)loaded,
               (int)result, result);
        D_write = true;
        M_read = true;
        break;
      case BV6(0, 0, 1, 1, 0, 0): // OPLUS D i
        address = unsigned_immediate;
        loaded = M[address];
        result = D ^ loaded;
        INSTRUCTION("OPLUS %s 0x%x", D_symbol, i);
        ACTION("%s = %s ^ M(<0x%x>) = 0x%x ^ 0x%x = 0x%x", D_symbol, D_symbol,
               i, D, loaded, result);
        D_write = true;
        M_read = true;
        break;
      case BV6(0, 0, 1, 1, 0, 1): // OR D i
        address = unsigned_immediate;
        loaded = M[address];
        result = D | loaded;
        INSTRUCTION("OR %s 0x%x", D_symbol, i);
        ACTION("%s = %s | M(<0x%x>) = 0x%x | 0x%x = 0x%x", D_symbol, D_symbol,
               i, D, loaded, result);
        D_write = true;
        M_read = true;
        break;
      case BV6(0, 0, 1, 1, 1, 0): // AND D i
        address = unsigned_immediate;
        loaded = M[address];
        result = D & loaded;
        INSTRUCTION("AND %s 0x%x", D_symbol, i);
        ACTION("%s = %s & M(<0x%x>) = 0x%x & 0x%x = 0x%x", D_symbol, D_symbol,
               i, D, loaded, result);
        D_write = true;
        M_read = true;
        break;
      case BV6(0, 0, 0, 0, 0, 0):
      case BV6(0, 0, 0, 0, 0, 1):
      case BV6(0, 0, 0, 1, 1, 1):
      case BV6(0, 0, 1, 0, 0, 0):
      case BV6(0, 0, 1, 0, 0, 1):
      case BV6(0, 0, 1, 1, 1, 1):
        die("illegal instruction '0x%08x' at 'code[0x%08x]'", I, PC);
        break;
      }
      break; // end of Compute Instructions

    case BV2(1, 1): // Jump Instructions
      switch (I31to27) {
      case BV5(1, 1, 0, 0, 0): // NOP
        INSTRUCTION("NOP");
        break;
      case BV5(1, 1, 0, 0, 1): // JUMP> i
        taken = ((int)ACC > 0);
        comparison = taken ? ">" : "<=";
        INSTRUCTION("JUMP> %d", signed_immediate);
        break;
      case BV5(1, 1, 0, 1, 0): // JUMP= i
        taken = ((int)ACC == 0);
        comparison = taken ? "=" : "!=";
        INSTRUCTION("JUMP= %d", signed_immediate);
        break;
      case BV5(1, 1, 0, 1, 1): // JUMP>= i
        taken = ((int)ACC >= 0);
        comparison = taken ? ">=" : "<";
        INSTRUCTION("JUMP>= %d", signed_immediate);
        break;
      case BV5(1, 1, 1, 0, 0): // JUMP< i
        taken = ((int)ACC < 0);
        comparison = taken ? "<" : ">=";
        INSTRUCTION("JUMP< %d", signed_immediate);
        break;
      case BV5(1, 1, 1, 0, 1): // JUMP!= i
        taken = ((int)ACC != 0);
        comparison = taken ? "!=" : "=";
        INSTRUCTION("JUMP!= %d", signed_immediate);
        break;
      case BV5(1, 1, 1, 1, 0): // JUMP<= i
        taken = ((int)ACC <= 0);
        comparison = taken ? "<=" : ">";
        INSTRUCTION("JUMP<= %d", signed_immediate);
        break;
      case BV5(1, 1, 1, 1, 1): // JUMP i
        taken = true;
        INSTRUCTION("JUMP %d", signed_immediate);
        break;
      }
      if (taken) {
        PC_next = PC + signed_immediate;
        if (comparison)
          ACTION("PC = PC + [0x%x] = %u %c %d = %u = 0x%x "
                 "as %d = [0x%x] = ACC %s 0",
                 i, PC, immediate_sign_char, abs_immediate, PC_next, PC_next,
                 (int)ACC, ACC, comparison);
        else
          ACTION("PC = PC + [0x%x] = %u %c %d = %u = 0x%x", i, PC,
                 immediate_sign_char, abs_immediate, PC_next, PC_next);
      } else if (comparison) {
        assert(comparison);
        assert(PC_next == PC + 1);
        ACTION("no jump as %d = [0x%x] = ACC %s 0", ACC, ACC, comparison);
      } else
        ACTION("%s", "");
      break; // end of Jump Instructions
    }

#ifndef NSTEPPING
    if (step) {
      if (steps == 1) {
        fputs("STEPS    PC       CODE     IN1      IN2      ACC      ", stdout);
        printf(instruction_format, "INSTRUCTION");
        fputs(" ACTION\n", stdout);
      }
      printf("%-8zu %08x %08x %08x %08x %08x ", steps, PC, I, IN1, IN2, ACC);
      printf(instruction_format, instruction);
#ifndef NDEBUG
      char instruction2[32];
      disassemble_reti_code(I, instruction2);
#endif
      fputc(' ', stdout);
      fputs(action, stdout);
      fputc('\n', stdout);
      fflush(stdout);
#ifndef NDEBUG
      if (strcmp(instruction, instruction2)) {
        fprintf(stderr,
                "emreti: fatal: "
                "disassambled instruction '%s' does not match\n",
                instruction2);
        fflush(stderr);
        abort();
      }
#endif
    }
#endif

    if (M_read) {
      if (address >= shadow.data || !shadow.valid[address]) {
        if (debug > 0) {
          warn("stopping on reading uninitialized 'data[0x%x]'", address);
	  break;
	}
        if (!debug)
          warn("continuing after uninitialized 'data[0x%x]' "
               "(use '-i' so squelch such messages, or '-g' to stop)",
               address);
      }
    }

    assert(!D_write || !M_write);

    // First write result to register if written.

    if (D_write) {

      *D_pointer = result;

      if (D_pointer == &reti.PC)
        PC_next = result;
    }

    // Then write result to memory if written.

    if (M_write) {

      if (address >= CAPACITY)
        die("can not write 'data[0x%x]' above address 0x%x", address,
            (unsigned)(CAPACITY - 1));

      // Written data becomes valid.

      if (!shadow.valid[address]) {
        shadow.valid[address] = true;
        if (address >= shadow.data)
          shadow.data = 1 + (size_t)address;
      }

      M[address] = result;
    }

    if (PC_next == PC) { // Check if stuck in infinite loop.
#ifndef NSTEPPING
      if (step) {
        if (steps == 1)
          fputs("STEPS   PC       CODE     IN1      IN2      ACC\n", stdout);
        printf("%-8zu %08x %08x %08x %08x %08x <infinite-loop>\n", steps, PC, I,
               IN1, IN2, ACC);
      }
#endif
      break;
    }

    // Finally update PC.

    reti.PC = PC_next;
  }

#ifndef NSTEPPING
  if (step)
    fputs("ADDRESS  DATA     BYTES       "
          "ASCII  UNSIGNED       SIGNED\n",
          stdout);
#endif

  for (size_t i = 0; i != shadow.data; i++)
    if (shadow.valid[i]) {
      const unsigned word = reti.data[i];
      printf("%08x %08x", (unsigned)i, word);
#ifndef NSTEPPING
      if (step) {
        for (unsigned i = 0, tmp = word; i != 4; i++, tmp >>= 8)
          printf(" %02x", tmp & 0xff);
        fputs(" ", stdout);
        for (unsigned i = 0, tmp = word; i != 4; i++, tmp >>= 8) {
          int ch = tmp & 0xff;
          printf("%c", isprint(ch) ? ch : '.');
        }
        printf("%11u %12d", (unsigned)word, (int)word);
      }
#endif
      fputc('\n', stdout);
    }

  free(shadow.valid);
  free(reti.data);
  free(reti.code);

  return 0;
}
