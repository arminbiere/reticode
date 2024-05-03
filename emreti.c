#include <assert.h>    // 'assert'
#include <ctype.h>     // 'isdigit'
#include <stdarg.h>    // va_list, vfprintf
#include <stdbool.h>   // 'bool'
#include <stdio.h>     // 'printf', 'snprintf', 'fputs', 'fputc'
#include <stdlib.h>    // 'malloc', 'calloc', 'free'
#include <string.h>    // 'strcmp'
#include <sys/stat.h>  // 'stat'
#include <sys/types.h> // needed by 'stat'
#include <unistd.h>    // needed by 'stat'

// On Linux allocating 2^32 unsigned words for the code and data memories
// succeeds as it actually only allocates virtual memory, which is mapped
// (really allocated) only when used.  On other platforms you might want to
// set the capacity closer to the actually needed memory.

// Yields 2^32 words = 16 GB for each the code and data memory.
//
// #define CAPACITY ((size_t)1 << 32)

// Yields 2^16 words = 256 KB for each the code and data memory.
//
#define CAPACITY ((size_t)1 << 32) // yields 2^16 words = 256 KB

// These 'BV' macros allow to generate constant bit-vectors of the given
// size at compile time.  Using functions would not work.

#define BV2(B1, B0) ((B1 << 1) | (B0 << 0))

#define BV4(B3, B2, B1, B0) ((B3 << 3) | (B2 << 2) | (B1 << 1) | (B0 << 0))

#define BV5(B4, B3, B2, B1, B0)                                                \
  ((B4 << 4) | (B3 << 3) | (B2 << 2) | (B1 << 1) | (B0 << 0))

#define BV6(B5, B4, B3, B2, B1, B0)                                            \
  ((B5 << 5) | (B4 << 4) | (B3 << 3) | (B2 << 2) | (B1 << 1) | (B0 << 0))

// Exit with error message with 'printf' style usage.
//
// The following declaration lets the compiler produce error messages if the
// arguments do not match format, i.e., if a string 'const char*' is given while
// '%d' is specified for the corresponding argument.

static void die(const char *, ...) __attribute__((format(printf, 1, 2)));

static void die(const char *fmt, ...) {
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
  fputs("emreti: warning: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  fflush(stderr);
}

// Check if the given path exists as file.

static bool file_exists(const char *path) {
  struct stat buf;
  return !stat(path, &buf);
}

int main(int argc, char **argv) {

  // First parse command line options.

#ifdef STEPPING
  bool step = false;
#endif

  const char *code_path = 0;
  const char *data_path = 0;

  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
      fputs("usage: emreti [ -h | --help", stdout);
#ifdef STEPPING
      fputs(" | -s | --step", stdout);
#endif
      fputs(" ] <code> <data>\n", stdout);
      exit(0);
    } else if (!strcmp(arg, "-s") || !strcmp(arg, "--step")) {
#ifdef STEPPING
      step = true;
#else
      die("invalid option '%s' (compiled without stepping support)", arg);
#endif
    } else if (arg[0] == '-')
      die("invalid option '%s' (try '-h')", arg);
    else if (!code_path)
      code_path = arg;
    else if (!data_path)
      data_path = arg;
    else
      die("more than two files specified '%s', '%s' and '%s' (try '-h')",
	  code_path, data_path, arg);
  }

  if (!code_path)
    die("no code file specified");
  if (!data_path)
    die("no data file specified");
  if (!file_exists(code_path))
    die("code file '%s' does not exist", code_path);
  if (!file_exists(data_path))
    die("data file '%s' does not exist", data_path);

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

  // Read code file.

  FILE *code_file = fopen(code_path, "r");
  if (!code_file)
    die("could not read code file '%s'", code_path);
  reti.code = malloc(CAPACITY * sizeof *reti.code);
  if (!reti.code)
    die("could not allocate code");
  shadow.code = 0;
  unsigned word;
  while (fread(&word, sizeof word, 1, code_file) == 1)
    if (shadow.code == CAPACITY)
      die("capacity of code area reached");
    else
      reti.code[shadow.code++] = word;
  fclose(code_file);

  // Read data file and set valid memory area.

  shadow.valid = calloc(CAPACITY, sizeof *shadow.valid);
  if (!shadow.valid)
    die("could not allocate valid");
  FILE *data_file = fopen(data_path, "r");
  if (!data_file)
    die("could not read data file '%s'", data_path);
  reti.data = malloc(CAPACITY * sizeof *reti.data);
  if (!reti.data)
    die("could not allocate data");
  shadow.data = 0;
  while (fread(&word, sizeof word, 1, data_file) == 1)
    if (shadow.data == CAPACITY)
      die("capacity of data area reached");
    else {
      shadow.valid[shadow.data] = true;
      reti.data[shadow.data] = word;
      shadow.data++;
    }
  fclose(data_file);

  // Simulate code on data.

#ifdef STEPPING // If the compile time flag 'STEPPING' is set.

  // Buffers for printing step information.

  // e.g., "SUBI ACC 0x123456"
  //
  char instruction[128];

  // e.g., "ACC = ACC - [0x123456] = 1193047 - 1193046 = 1 = [0x00000001]"
  //
  char action[128];

#define INSTRUCTION(...)                                                       \
  do {                                                                         \
    if (step)                                                                  \
      snprintf(instruction, 128, __VA_ARGS__);                                 \
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

  while (reti.PC < shadow.code) {

    const unsigned PC = reti.PC;
    if (PC >= shadow.code) {
      warn("stopping at undefined 'code[0x%08x]' above 0x%08x", PC,
	   (unsigned)(shadow.code - 1));
      break;
    }
    const unsigned I = reti.code[PC];

    const unsigned I31to30 = I >> 30;
    const unsigned I31to28 = I >> 28;
    const unsigned I31to26 = I >> 26;
    const unsigned I31to27 = I >> 28;
    const unsigned I23toI0 = I & 0xffffff;
    const unsigned I25to24 = (I >> 24) & 3;
    const unsigned I27to26 = (I >> 26) & 3;

    const unsigned i = I23toI0;
    const unsigned unsigned_immediate = i;
    const unsigned immediate_sign_bit = (i >> 23) & 1;
    const unsigned immediate_extension = immediate_sign_bit ? 0xff000000 : 0;
    unsigned signed_immediate = immediate_extension | unsigned_immediate;

    // Get content of source register and its symbolic name (in any case).

    unsigned S = 0;
    const char *S_symbol = 0;

    switch (I27to26) {
    case BV2(0, 0):
      S = reti.PC;
      S_symbol = "PC";
      break;
    case BV2(0, 1):
      S = reti.IN1;
      S_symbol = "IN1";
      break;
    case BV2(1, 0):
      S = reti.IN2;
      S_symbol = "IN2";
      break;
    case BV2(1, 1):
      S = reti.ACC;
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

    unsigned PC_next = PC + 1; // Default is to increase PC.
    bool D_write = false;      // Default is not to write to register D.
    bool M_write = false;      // Default is not to write to memory.
    bool M_read = false;       // Default is not to read from memory.
    unsigned result = 0;       // Computed, loaded, or stored result.
    unsigned address = 0;      // Address to read from or write to memory.

    unsigned *M = reti.data;

#ifdef STEPPING

    // Just make sure to have a valid string (with terminating zero).

    instruction[0] = action[0] = 0;

#endif

    // Now we decode the actual instructions and execute it.

    switch (I31to30) {

    case BV2(0, 1): // Load Instructions
      switch (I31to28) {
      case BV4(0, 1, 0, 0): // LOAD D i
	address = unsigned_immediate;
	result = M[address];
	INSTRUCTION("LOAD %s %u", S_symbol, i);
	ACTION("%s = M(<0x%x>) = M(0x%x) = 0x%x", S_symbol, i, address, result);
	M_read = true;
	D_write = true;
	break;
      case BV4(0, 1, 0, 1): // LOADIN1 D i
	break;
      case BV4(0, 1, 1, 0): // LOADIN2 D i
	break;
      case BV4(0, 1, 1, 1): // LOADI D i
	result = unsigned_immediate;
	INSTRUCTION("LOADI %s %u", S_symbol, i);
	ACTION("%s = 0x%x", S_symbol, i);
	D_write = true;
	break;
      }
      break; // end of Load Instructions

    case BV2(1, 0): // Store Instructions
      switch (I31to28) {
      case BV4(1, 0, 0, 0): // STORE i
	address = unsigned_immediate;
	result = S;
	INSTRUCTION("STORE %u", i);
	ACTION("M(<%u>) = M(0x%x) = 0x%x", i, address, result);
	M_write = true;
	break;
      case BV4(1, 0, 0, 1): // STOREIN1 i
	break;
      case BV4(1, 0, 1, 0): // STOREIN2 i
	break;
      case BV4(1, 0, 1, 1): // MOVE S D
	break;
      }
      break; // end of Store Instructions

    case BV2(0, 0): // Compute Instructions
      switch (I31to26) {
	unsigned result;
      case BV6(0, 0, 0, 0, 1, 0): // SUBI D i
	result = S - signed_immediate;
	INSTRUCTION("SUBI %s %d", S_symbol, signed_immediate);
	ACTION("%s = %s - [0x%x] = %d - %d = %d = [0x%x]", D_symbol, S_symbol,
	       i, (int)S, (int)i, (int)result, result);
	D_write = true;
	break;
      case BV6(0, 0, 0, 0, 1, 1): // ADDI D i
	result = S + signed_immediate;
	INSTRUCTION("ADDI %s %d", S_symbol, signed_immediate);
	ACTION("%s = %s + [0x%x] = %d + %d = %d = [0x%x]", D_symbol, S_symbol,
	       i, (int)S, (int)i, (int)result, result);
	D_write = true;
	break;
      case BV6(0, 0, 0, 1, 0, 0): // OPLUSI D i
	result = S ^ unsigned_immediate;
	INSTRUCTION("OPLUSI %s 0x%x", S_symbol, i);
	ACTION("%s = %s ^ 0x%x = 0x%x ^ 0x%x = 0x%x", D_symbol, S_symbol,
	       unsigned_immediate, S, unsigned_immediate, result);
	D_write = true;
	break;
      case BV6(0, 0, 0, 1, 0, 1): // ORI D i
	result = S | unsigned_immediate;
	INSTRUCTION("ORI %s 0x%x", S_symbol, i);
	ACTION("%s = %s | 0x%x = 0x%x | 0x%x = 0x%x", D_symbol, S_symbol,
	       unsigned_immediate, S, unsigned_immediate, result);
	D_write = true;
	break;
      case BV6(0, 0, 0, 1, 1, 0): // ANDI D i
	result = S & unsigned_immediate;
	INSTRUCTION("ANDI %s 0x%x", S_symbol, i);
	ACTION("%s = %s & 0x%x = 0x%x & 0x%x = 0x%x", D_symbol, S_symbol,
	       unsigned_immediate, S, unsigned_immediate, result);
	D_write = true;
	break;
      case BV6(0, 0, 1, 0, 1, 0): // SUB D i
	break;
      case BV6(0, 0, 1, 0, 1, 1): // ADD D i
	break;
      case BV6(0, 0, 1, 1, 0, 0): // OPLUS D i
	break;
      case BV6(0, 0, 1, 1, 0, 1): // OR D i
	break;
      case BV6(0, 0, 1, 1, 1, 0): // AND D i
	break;
      }
      break; // end of Compute Instructions

    case BV2(1, 1): // Jump Instructions
      switch (I31to27) {
      case BV5(1, 1, 0, 0, 0): // NOP
	INSTRUCTION("NOP");
	ACTION(" ");
	break;
      case BV5(1, 1, 0, 0, 1): // JUMP> i
	break;
      case BV5(1, 1, 0, 1, 0): // JUMP= i
	break;
      case BV5(1, 1, 0, 1, 1): // JUMP>= i
	break;
      case BV5(1, 1, 1, 0, 0): // JUMP< i
	break;
      case BV5(1, 1, 1, 0, 1): // JUMP!= i
	break;
      case BV5(1, 1, 1, 1, 0): // JUMP<= i
	break;
      case BV5(1, 1, 1, 1, 1): // JUMP i
	break;
      }
      break; // end of Jump Instructions
    }

    if (M_read) {
      if (address >= shadow.data || !shadow.valid[address])
	warn("read uninitialized 'data[0x%x]'", address);
    }

#ifdef STEPPING
    if (step) {
      fprintf(stderr, "PC=0x%08x IN1=0x%08x IN2=0x%08x ACC=0x%08x  ", reti.PC,
	      reti.IN1, reti.IN2, reti.ACC);
      fprintf(stderr, "%-18s", instruction);
      fputs(" : ", stderr);
      fputs(action, stderr);
      fputc('\n', stderr);
      fflush(stdout);
    }
#endif

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

    // Finally update PC.

    reti.PC = PC_next;

    if (PC_next == PC)
      break;
  }

  for (size_t i = 0; i != shadow.data; i++)
    if (shadow.valid[i]) {
      printf("%08x ", (unsigned)i);
      const unsigned word = reti.data[i];
      for (unsigned i = 0, tmp = word; i != 4; i++, tmp >>= 8)
	printf(" %02x", tmp & 0xff);
      fputs("  ", stdout);
      for (unsigned i = 0, tmp = word; i != 4; i++, tmp >>= 8) {
	int ch = tmp & 0xff;
	printf("%c", isprint(ch) ? ch : '.');
      }
      fputc('\n', stdout);
    }

  free(shadow.valid);
  free(reti.data);
  free(reti.code);

  return 0;
}
