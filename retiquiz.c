// clang-format off

static const char * usage =
"usage: retiquiz [ <option> ... ] [ <seed> ] [ <questions> ]\n"
"\n"
"where '<option>' is one of the following\n"
"\n"
"  -h | --help             print this command line option summary\n"
"  -n | --non-interactive  only prints questions\n"
"\n"
"This tool generates questions around the ReTI assembler language.\n"
"By default '16' random questions are asked (set with '<questions>').\n"
"If seed is '-' then still a random seed is generated which is useful\n"
"if a different number of questions is needed.\n"
;

// clang-format on

#include "disreti.h"

#include <ctype.h>     // isdigit
#include <inttypes.h>  // PRIu64
#include <limits.h>    // UINT_MAX
#include <stdarg.h>    // va_list, va_start, vfprintf
#include <stdint.h>    // uint64_t
#include <stdio.h>     // fputs, printf
#include <stdlib.h>    // exit
#include <sys/time.h>  // gettimeofday
#include <sys/times.h> // tms, times
#include <sys/types.h> // getpid
#include <termios.h>   // tcgetattr, tcsetattr
#include <unistd.h>    // getpid

// Terminal color escape codes.

#define BOLD "\033[1m"
#define GREEN "\033[32m"
#define HEADER "\033[35m"
#define NORMAL "\033[0m"
#define OTHER "\033[33m"
#define RED "\033[31m"
#define WHITE "\033[34m"

// Unicode of OK and XX (failure).

#define OK "✓"
#define XX "✗"

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

// Global options.

static bool interactive = true;

// Statistics.

static volatile uint64_t ask;
static volatile uint64_t asked;
static volatile uint64_t answered;
static volatile uint64_t skipped;
static volatile uint64_t correct;
static volatile uint64_t incorrect;

// Raw terminal set

static struct termios original;

static void reset_terminal(void) {
  assert(interactive);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
}

static void init_terminal(void) {
  if (!interactive)
    return;
  tcgetattr(STDIN_FILENO, &original);
  atexit(reset_terminal);
  struct termios raw = original;
  raw.c_lflag &= ~(ECHO | ICANON);
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

static double wall_clock_time(void) {
  struct timeval tv;
  if (gettimeofday(&tv, 0))
    return 0;
  return 1e-6 * tv.tv_usec + tv.tv_sec;
}

static double percent(double a, double b) { return 100 * (b ? a / b : 0); }

static void color(const char *color_code) {
  if (interactive)
    fputs(color_code, stdout);
}

int main(int argc, char **argv) {

  // First parse options and get seed and questions strings.

  const char *seed_string = 0;
  const char *questions_string = 0;

  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h") || !strcmp(arg, "--help")) {
      fputs(usage, stdout);
      exit(0);
    } else if (!strcmp(arg, "-n") || !strcmp(arg, "--non-interactive"))
      interactive = false;
    else if (arg[0] == '-' && arg[1])
      die("invalid option '%s' (try '-h')", arg);
    else if (!seed_string)
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
    generator = (uint64_t)times(&tp); // Use time.
    (void)random64();                 // Hash time.
    generator ^= (uint64_t)getpid();  // Mix in process identifier.
    (void)random64();                 // Hash both.
    seed = generator;
  }

  // Parse questions argument.

  if (questions_string) {
    const char *p = questions_string;
    int ch = *p++;
    if (!isdigit(ch))
      die("invalid number of questions '%s'", questions_string);
    ask = ch - '0';
    const uint64_t max_questions = (uint64_t)1 << 32;
    while ((ch = *p++)) {
      if (!isdigit(ch))
        die("invalid number of questions '%s'", questions_string);
      if (max_questions / 10 < ask)
        die("number of questions '%s' exceed maximum", questions_string);
      ask *= 10;
      int digit = ch - '0';
      if (max_questions - digit < ask)
        die("number of questions '%s' exceed maximum", questions_string);
      ask += digit;
    }
  } else
    ask = 16;

  generator = seed;
  init_terminal();

  double start_time = wall_clock_time();

  if (interactive) {
    color(HEADER);
    printf("ReTI Machine Code Quiz Version " VERSION "\n");
    color(NORMAL);
  }
  printf("retiquiz %" PRIu64 " %" PRIu64 "\n", seed, ask);
  if (interactive) {
    printf("Enter hexadecimal digits as an answer or\n");
    printf("space ' ' to skip a question or 'q' to quit.\n");
    printf("For irrelevant '*' in the machine code use '0'.\n");
    printf("Asking %" PRIu64 " questions.\n", ask);
    color(HEADER);
    printf("INSTRUCTION         ; PC       CODE\n");
    color(NORMAL);
  } else {
    printf("INSTRUCTION         ; PC       QUERY    SOLUTION     CODE\n");
  }

  char instruction[disassembled_reti_code_length];
  char answer[disassembled_reti_code_length];
  char expected[9], query[9];

  uint64_t pc = 0;

  while (asked != ask) {

    // Generate a really random 32-bit code word first.

    unsigned code = random32();

    // Restrict immedidates to small negative and positive numbers.

    const unsigned type = code >> 30;
    const unsigned mode = (code >> 28) & 3;
    const unsigned comparison = (code >> 27) & 7;

    if (type != 1 && type != 2 && (code & 0x00800000))
      code |= 0x00ffffe0;
    else
      code &= 0xff00001f;

    // Force irrelevant '*' to '0'.

    if (type == 1)         // LOAD
      code &= ~0x0c000000; // force S to zero
    if (type == 2) {
      if (mode == 3)         // MOVE
        code &= 0xff000000;  // force immediate to zero
      else                   // STORE
        code &= ~0x0f000000; // force S and D to zero
    }
    if (type == 3) {       // JUMP
      code &= ~0x07000000; // force the 3 bits to zero
      if (comparison == 0 || comparison == 7)
        code &= 0xff000000; // force zero immediate
    }

    // Now disassamble for printing.

    if (!disassemble_reti_code(code, instruction))
      continue;

    // We are now ready to present the 'query'.

    asked++;
    sprintf(expected, "%08x", code);
    strcpy(query, expected);

    // Also restrict the position of '_' depending on instruction.

    unsigned pos;
    if (code & 0x00800000) // something with a negative immediate
      pos = pick32(0, 7);
    else if (type == 2) {
      if (mode == 3) // MOVE thus only first two nibbles.
        pos = pick32(0, 1);
      else { // STORE
        pos = pick32(0, 2);
        if (pos)
          pos += 5;
      }
    } else {
      pos = pick32(0, 3);
      if (type == 3 && (comparison == 0 || comparison == 7))
        pos &= 1;
      else {
        assert(pos < 4);
        if (pos > 1)
          pos += 4;
      }
    }
    assert(pos < 8);

    // Overwrite query character at 'pos' with the template '_'.

    query[pos] = '_';
    printf("%-19s ; %08x %s", instruction, (unsigned)pc++, query);

    // In non-interative mode we just print the solution and continue.

    if (!interactive) {
      printf("     %c    %s\n", expected[pos], expected);
      continue;
    }

    // For interactive mode we go backward with '\b' to the position of '_'.

    for (unsigned i = 0; i != 8 - pos; i++)
      fputc('\b', stdout);
    fflush(stdout);

    // And now read from the terminal a character.

    int ch;
  READ1:;
    ssize_t chars = read(STDIN_FILENO, &ch, 1);

    // Just quit if we failed to read a character or read 'q'.

    if (chars != 1 || ch == 'q') {
      fputc('\n', stdout);
      fflush(stdout);
      break;
    }

    if (ch == ' ') {
      skipped++;
      color(OTHER);
      fputc('_', stdout);
      color(NORMAL);
      fputs(query + pos + 1, stdout);
      fputc('\n', stdout);
      continue;
    }

    // We only allow hexadecimal digits.

    unsigned nibble = 0;
    if ('A' <= ch && ch <= 'F') {
      ch = ch - 'A' + 'a';
      nibble = ch - 'a' + 10;
    } else if ('a' <= ch && ch <= 'f')
      nibble = ch - 'a' + 10;
    else if ('0' <= ch && ch <= '9') {
      nibble = ch - '0';
    } else {
      fputc('\a', stdout); // Alert and read next character.
      fflush(stdout);
      goto READ1;
    }

    // Compute the answered code.

    answered++;
    unsigned shift = (7 - pos) * 4;
    unsigned answer_code = code & ~(0xf << shift);
    answer_code |= nibble << shift;

    // Disassamble and check that it produces the same assembler.

    bool matched = disassemble_reti_code(answer_code, answer) &&
                   !strcmp(instruction, answer);

    color(matched ? GREEN : RED);
    fputc(ch, stdout);
    color(NORMAL);
    fputs(query + pos + 1, stdout);
    fputc(' ', stdout);
    fputs(matched ? GREEN : RED, stdout);
    fputs(matched ? OK : XX, stdout);

    if (matched)
      correct++;
    else {
      incorrect++;
      color(OTHER);
      fputs("  expected ", stdout);
      color(GREEN);
      fputc(expected[pos], stdout);
      color(OTHER);
      fputs(" in ", stdout);
      color(BOLD);
      unsigned i = 0;
      while (i != pos)
        fputc(expected[i++], stdout);
      color(GREEN);
      fputc(expected[pos], stdout);
      color(OTHER);
      while (++i != 8)
        fputc(expected[i], stdout);
      unsigned low = 4 * (7 - pos);
      unsigned hi = low + 3;
      color(NORMAL);
      color(OTHER);
      fputs(" at ", stdout);
      color(NORMAL);
      color(WHITE);
      printf("I[%u:%u]", hi, low);
    }

    color(NORMAL);
    fputc('\n', stdout);
    fflush(stdout);
  }

  if (interactive) {
    color(HEADER);
    printf("RESULT\n");
    color(NORMAL);
    printf("asked       %3.0f%% %4" PRIu64 "/%" PRIu64 "\n",
           percent(asked, ask), asked, ask);
    printf("answered    %3.0f%% %4" PRIu64 "/%" PRIu64 "\n",
           percent(answered, asked), answered, asked);
    printf("skipped     %3.0f%% %4" PRIu64 "/%" PRIu64 "\n",
           percent(skipped, asked), skipped, asked);
    printf("correct   ");
    color(GREEN);
    fputs(OK, stdout);
    color(NORMAL);
    printf(" %3.0f%% %4" PRIu64 "/%" PRIu64 "\n", percent(correct, asked),
           correct, asked);
    printf("incorrect ");
    color(RED);
    fputs(XX, stdout);
    color(NORMAL);
    printf(" %3.0f%% %4" PRIu64 "/%" PRIu64 "\n", percent(incorrect, asked),
           incorrect, asked);

    color(HEADER);
    printf("POINTS\n");
    color(NORMAL);
    if (correct < incorrect)
      printf("0 points   (more answers incorrect than correct)\n");
    else {
      uint64_t points = correct - incorrect;
      printf("%" PRIu64 " points %.0f%%   (%" PRIu64 " correct - %" PRIu64
             " incorrect)\n",
             points, percent(correct, ask), correct, incorrect);
    }

    double end_time = wall_clock_time();
    double seconds = end_time - start_time;
    color(HEADER);
    printf("TIME\n");
    color(NORMAL);
    printf("%.2f seconds\n", seconds);

    reset_terminal();
  }

  return 0;
}
