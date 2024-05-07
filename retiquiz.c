// clang-format off

static const char * usage =
"usage: retiquiz [ <option> ... ] [ <seed> ] [ <questions> ]\n"
"\n"
"where '<option>' is one of the following\n"
"\n"
"  -h | --help         print this command line option summary\n"
"\n"
"This tool generates questions around the ReTI assembler language.\n"
"By default it asks '10' random questions (can be set with '<questions>')\n"
"and prints a solution after the user has entered an answer.  At the end\n"
"the number of correctly solved questions is printed.\n"
;

// clang-format on

#include "disreti.h"

#include <ctype.h>     // isdigit
#include <inttypes.h>  // PRIu64
#include <stdarg.h>    // va_list, va_start, vfprintf
#include <stdint.h>    // uint64_t
#include <stdio.h>     // fputs, printf
#include <stdlib.h>    // exit
#include <sys/times.h> // tms, times

static void die(const char *, ...) __attribute__((format(printf, 1, 2)));

static void die(const char *fmt, ...) {
  fputs("retiquiz: error: ", stderr);
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

static volatile struct {
  uint64_t ask;
  uint64_t asked;
  uint64_t answered;
  uint64_t correct;
  uint64_t incorrect;
} questions;

int main(int argc, char **argv) {

  // First parse options and get seed and questions strings.

  const char *seed_string = 0;
  const char *questions_string = 0;

  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
      fputs(usage, stdout);
      exit(0);
    } else if (!seed_string)
      seed_string = arg;
    else if (!questions_string)
      questions_string = arg;
    else
      die("too many arguments '%s', '%s' and '%s'", seed_string,
	  questions_string, arg);
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

  // Parse questions argument.

  if (questions_string) {
    const char *p = questions_string;
    int ch = *p++;
    if (!isdigit(ch))
      die("invalid number of questions '%s'", questions_string);
    uint64_t ask = ch - '0';
    const uint64_t max_questions = (uint64_t)1 << 32;
    while ((ch = *p++)) {
      if (!isdigit(ch))
	die("invalid number of questions '%s'", questions_string);
      if (max_questions / 10 < questions)
	die("number of questions '%s' exceed maximum", questions_string);
      questions *= 10;
      int digit = ch - '0';
      if (max_questions - digit < questions)
	die("number of questions '%s' exceed maximum", questions_string);
      questions += digit;
    }
    questions.ask = ask;
  }

  printf("retiquiz %" PRIu64 " %" PRIu64 "\n", seed, questions.ask);

  return 0;
}
