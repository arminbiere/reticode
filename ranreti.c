// clang-format off

static const char * usage =
"usage: ranreti [ <option> ... ] [ <seed> ] [ <instructions> ] ]\n"
"\n"
"where '<opion>' is one of the following\n"
"\n"
"  -h | --help   print this command line option summary\n"
"\n"

"and '<seed>' gives starting seed of the random number generator.\n"
"The default is to use random seed taking process identifier and time\n"
"into account.  The number of instructions generated is picked randomly too\n"
"in the range '1..32' unless '<instructions>' is specified explicitly.\n"
"If '<instructions>' has a learing '-' it is uniformly picked in that range.\n"
"A single positive number is a seed and a single negative gives the the\n"
"limit on the number of generated instruction.  With '-' insead of '<seed>'\n"
"we specify picking a random seed.\n"
"\n"
"Here are some examples:\n"
"\n"
"  ranreti       # generate random ReTI program of length '1..32'\n"
"  ranreti 1     # set seed to '1' and use random number of instructions\n"
"  ranreti 1 10  # set seed to '1' too and generate exactly 10 instructions\n"
"  ranreti 1 -10 # set seed to '1' and limit instructions to at most 10\n"
"  ranreti -10   # random seed and limit instructions to at most 10\n"
"  ranreti - 10  # random seed and exactly 10 instructions\n"
"  ranreti -     # redundant (same as not specifying '-')\n"
"\n"
"The machine code of each instruction is generated randomly without illegal\n"
"instructions and jumps are forced to not yield an infinite loop and to not\n"
"jump out of the program beyond right after the end of the program.\n"
;

// clang-format on

#include "disreti.h" // disassemble_reti_code

#include <assert.h>   // assert
#include <ctype.h>    // isdigit
#include <inttypes.h> // PRIu64
#include <limits.h>   // UINT_MAX
#include <stdarg.h>   // va_list, va_start, vfprintf, va_end
#include <stdint.h>   // uint64_t
#include <stdio.h>    // fprintf, printf,
#include <stdlib.h>   // exit,
#include <string.h>   // strcmp

#include <sys/times.h> // times
#include <sys/types.h> // getpid
#include <unistd.h>    // getpid

static void die(const char *, ...) __attribute__((format(printf, 1, 2)));

static void die(const char *fmt, ...) {
  fputs("ranreti: error: ", stderr);
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

static uint64_t generator; // State of random number generate.

// Long period generator of Donald Knuth with linear congruential method.

static uint64_t random64(void) {
  generator *= 6364136223846793005ul;
  generator += 1442695040888963407ul;
  return generator;
}

// Lower 32-bits are better.

static unsigned random32(void) { return random64() >> 32; }

// Pick a random number interval from 'l' to 'r' including both limits.
// Use floating point as modulo is imprecise.

static unsigned pick32(unsigned l, unsigned r) {
  assert(l <= r);
  if (l == r)
    return l;
  const unsigned delta = r - l + 1;
  const unsigned tmp = random32();
  unsigned res;
  if (!delta) {
    assert(!l);
    assert(r == UINT_MAX);
    res = tmp;
  } else {
    const double fraction = tmp / 4294967296.0;
    assert(0 <= fraction), assert(fraction < 1);
    const unsigned scaled = delta * fraction;
    assert(scaled < delta);
    res = l + scaled;
  }
  assert(l <= res), assert(res <= r);
  return res;
}

// Random bit also should use the floating point version.

static bool random1(void) { return pick32(0, 1); }

int main(int argc, char **argv) {

  // First parse options and get seed and instructions strings.

  const char *seed_string = 0;
  const char *instructions_string = 0;

  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
      fputs(usage, stdout);
      exit(0);
    } else if (!seed_string)
      seed_string = arg;
    else if (!instructions_string)
      instructions_string = arg;
    else
      die("too many argument '%s', '%s' and '%s'", seed_string,
          instructions_string, arg);
  }

  // Normalize single argument (see 'usage' above).

  if (seed_string && !instructions_string) {
    if (!strcmp(seed_string, "-")) {
      // Redundant single '-' so drop it.
      seed_string = 0;
    } else if (seed_string[0] == '-') {
      // Use negative seed string to specify instructions and drop seed.
      instructions_string = seed_string;
      seed_string = 0;
    }
  }

  // Parse seed string or set to random seed.

  uint64_t seed = 0;

  if (seed_string && strcmp(seed_string, "-")) {

    const uint64_t max_seed = ~(uint64_t)0;
    if (!*seed_string)
      die("invalid empty seed string");
    for (const char *p = seed_string; *p; p++) {
      int ch = *p;
      if (!isdigit(ch))
        die("invalid seed '%s'", seed_string);
      if (max_seed / 10 < seed)
        die("seed '%s' exceeds maximum", seed_string);
      seed *= 10;
      int digit = ch - '0';
      if (max_seed - digit < seed)
        die("seed '%s' exceeds maximum", seed_string);
      seed += digit;
    }
  } else {
    struct tms tp;
    seed = 1111111121 * (uint64_t)times(&tp); // Spread time over 64-bits.
    seed += 20000003 * (uint64_t)getpid();    // Hash in process identifier.
  }

  // Parse instructions argument.

  uint64_t instructions;
  generator = seed;

  if (instructions_string) {
    const char *p = instructions_string;
    int ch = *p++;
    if (!ch)
      die("invalid empty instructions");
    if (ch == '-') {
      ch = *p++;
      if (!ch)
        die("invalid instructions '-'");
    }
    if (!isdigit(ch))
      die("invalid instructions '%s'", instructions_string);
    instructions = ch - '0';
    const uint64_t max_instructions = (uint64_t)1 << 32;
    while ((ch = *p++)) {
      if (!isdigit(ch))
        die("invalid instructions '%s'", instructions_string);
      if (max_instructions / 10 < instructions)
        die("instructions '%s' exceed maximum", instructions_string);
      instructions *= 10;
      int digit = ch - '0';
      if (max_instructions - digit < instructions)
        die("instructions '%s' exceed maximum", instructions_string);
      instructions += digit;
    }
    if (*instructions_string == '-') {
      if (instructions >= UINT_MAX)
        instructions = random32();
      else
        instructions = pick32(0, instructions);
    }
  } else {
    unsigned log_instructions = pick32(0, 5);
    instructions = pick32(1, (1u << log_instructions));
  }

  generator = seed;

  printf("; ranreti %" PRIu64 " %" PRIu64 "\n", seed, instructions);

  char str[disassembled_reti_code_length];
  for (uint64_t pc = 0; pc != instructions; pc++) {

    unsigned code = random32(); // Generate arbitrary random code.

    // For jumps we want to make sure that they stay within the
    // generated instructions.

    if (code > 0xc0000000) { // 'JUMP..' > 1100 0000 0000 0000 = 'NOP'

      uint64_t min_pc, max_pc;

      if (pc && random1()) { // Backward jump.
        min_pc = (pc >= 0x800000) ? pc - 0x800000 : 0;
        max_pc = pc - 1;
      } else { // Forward jump.
        min_pc = pc + 1;
        max_pc = pc + 0x7fffff;
        if (max_pc > instructions)
          max_pc = instructions; // Can point right after last instruction.
      }

      const unsigned min_jump = min_pc - pc;
      const unsigned max_jump = max_pc - pc;
      const unsigned immediate = pick32(min_jump, max_jump);

      code &= ~0xffffff;            // Clear immediate bits.
      code |= immediate & 0xffffff; // Add new randome immediate.
    }

    if (!((code >> 24) & 3))
      code |= pick32 (1,3) << 24;

    if (disassemble_reti_code(code, str)) {
      printf("%-21s ; %08x %08x\n", str, (unsigned)pc, code);
    }
  }
  return 0;
}
