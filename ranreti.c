// clang-format off

static const char * usage =
"usage: ranreti [ <option> ... ] [ <seed> [ <instructions> ] ]\n"
"\n"
"where '<opion>' is one of the following\n"
"\n"
"  -h | --help   print this command line option summary\n"
"\n"
"and '<seed>' gives starting seed of the random number generator (default\n"
"is '0').  The number of instrcutions generated is picked randomly too\n"
"in the range 1..1024 unless '<instructions>' is specified explicitly.\n"
"If '<instructions>' is negative it is uniformly picked in that range.\n"
;

// clang-format on

#include "disreti.h"

#include <ctype.h>
#include <inttypes.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

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

static uint64_t generator;

static uint64_t next_random(void) {
  generator *= 6364136223846793005ul;
  generator += 1442695040888963407ul;
  return generator;
}

static uint64_t pick_random(unsigned l, unsigned r) {
  assert(l <= r);
  if (l == r)
    return l;
  const unsigned delta = r - l;
  const unsigned tmp = next_random();
  const double fraction = tmp / 4294967296.0;
  assert(0 <= fraction), assert(fraction < 1);
  const unsigned scaled = delta * fraction;
  assert(scaled < delta);
  const unsigned res = l + scaled;
  assert(l <= res), assert(res < r);
  return res;
}

int main(int argc, char **argv) {

  // First some option parsing.

  const char *seed_string = 0;
  const char *instructions_string = 0;

  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
      fputs(usage, stdout);
      exit(0);
    } else if (arg[0] == '-' && !seed_string)
      die("invalid option '%s' (try '-h')", arg);
    else if (!seed_string)
      seed_string = arg;
    else if (!instructions_string)
      instructions_string = arg;
    else
      die("too many argument '%s', '%s' and '%s'", seed_string,
	  instructions_string, arg);
  }

  // Parse seed argument.

  uint64_t seed = 0;

  if (seed_string) {
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
      if (instructions == max_instructions)
	instructions = (next_random() >> 32);
      else {
	assert(instructions <= UINT_MAX);
	instructions = pick_random(0, instructions);
      }
    }
  } else {
    unsigned log_instructions = pick_random(0, 10);
    instructions = pick_random(1, (1u << log_instructions));
  }

  assert(instructions);

  printf("; ranreti %" PRIu64 " %" PRIu64 "\n", seed, instructions);

  char str[disassembled_reti_code_length];
  for (uint64_t pc = 0; pc != instructions; pc++) {
    unsigned code = (unsigned)rand() ^ (unsigned)(rand() << 16);
    if (disassemble_reti_code(code, str)) {
      printf("%-21s ; %08x %08x\n", str, (unsigned)pc, code);
    }
  }
  return 0;
}
