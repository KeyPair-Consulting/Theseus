#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <math.h>
#include <errno.h>

int main(int argc, char *argv[]) {
  char *endptr;
  unsigned long long int inint;
  double indouble;
  unsigned int sigfigs;
  char fmt[256];

  if(argc != 3) {
    fprintf(stderr, "sigfigs <num> <significant terms>\n");
    fprintf(stderr, "Got %d parameters.\n", argc);
    exit(EX_NOINPUT);
  }

  endptr = NULL;
  indouble = strtod(argv[1], &endptr);

  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wfloat-equal"
  //These double values are flags set by "strtod", so exact comparison is correct.
  if ((endptr == NULL) || (*endptr != '\0') || ((indouble == 0.0) && (endptr == argv[1])) || (((indouble == HUGE_VAL) || (indouble == -HUGE_VAL)) && errno == ERANGE)) {
    fprintf(stderr, "Can't interpret target double value\n");
    exit(EX_DATAERR);
  }
  #pragma GCC diagnostic pop

  endptr = NULL;
  inint = strtoull(argv[2], &endptr, 0);

  if((inint == 0) || (inint > 22) || (endptr == NULL) || (*endptr != '\0')) {
    fprintf(stderr, "Invalid number of significant figures.\n");
    exit(EX_DATAERR);
  }

  sigfigs = (unsigned int) inint;

  sprintf(fmt, "%%.0%ug", sigfigs);
  printf(fmt, indouble);
  printf("\n");
  return 0;
}
