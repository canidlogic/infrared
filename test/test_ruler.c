/*
 * test_ruler.c
 * ============
 * 
 * Test program for the ruler module of Infrared.
 * 
 * Syntax
 * ------
 * 
 *   test_ruler [slot] [gap] [test_beat] [test_grace]
 * 
 * The [slot] and [gap] parameters are signed integers that are used to
 * construct a ruler object.
 * 
 * The [test_beat] and [test_grace] parmeters are signed integers used
 * to test ruler calculations.  [test_beat] is the time offset in
 * subquanta of a beat and [test_grace] is the grace note index.
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Infrared modules:
 * 
 *   - diagnostic.c
 *   - ruler.c
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "diagnostic.h"
#include "ruler.h"

/*
 * Diagnostics
 * ===========
 */

static void raiseErr(int lnum, const char *pDetail, ...) {
  va_list ap;
  va_start(ap, pDetail);
  diagnostic_global(1, __FILE__, lnum, pDetail, ap);
  va_end(ap);
}

static void sayWarn(int lnum, const char *pDetail, ...) {
  va_list ap;
  va_start(ap, pDetail);
  diagnostic_global(0, __FILE__, lnum, pDetail, ap);
  va_end(ap);
}

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static int32_t parseInt(const char *pstr);

/*
 * Parse a given string as a signed decimal integer.
 * 
 * The least negative value in two's-complement is not supported.
 * 
 * Parameters:
 * 
 *   pstr - the string to parse
 * 
 * Return:
 * 
 *   the parsed integer value
 */
static int32_t parseInt(const char *pstr) {
  
  int32_t result = 0;
  int32_t smul = 1;
  int d = 0;
  
  if (pstr == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if (pstr[0] == '+') {
    smul = 1;
    pstr++;
  } else if (pstr[0] == '-') {
    smul = -1;
    pstr++;
  }
  
  if ((*pstr < '0') || (*pstr > '9')) {
    raiseErr(__LINE__, "Invalid integer program argument");
  }
  
  for( ; *pstr != 0; pstr++) {
    d = *pstr;
    if ((d >= '0') && (d <= '9')) {
      d = d - '0';
    } else {
      raiseErr(__LINE__, "Invalid integer program argument");
    }
    
    if (result <= INT32_MAX / 10) {
      result *= 10;
    } else {
      raiseErr(__LINE__, "Integer program argument out of range");
    }
    
    if (result <= INT32_MAX - d) {
      result += ((int32_t) d);
    } else {
      raiseErr(__LINE__, "Integer program argument out of range");
    }
  }
  
  result = result * smul;
  return result;
}

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  int32_t arg_slot = 0;
  int32_t arg_gap = 0;
  int32_t arg_beat = 0;
  int32_t arg_grace = 0;
  
  int32_t result = 0;
  
  RULER *pr = NULL;
  
  diagnostic_startup(argc, argv, "test_ruler");
  
  if (argc != 5) {
    raiseErr(__LINE__, "Wrong number of program arguments");
  }
  
  arg_slot  = parseInt(argv[1]);
  arg_gap   = parseInt(argv[2]);
  arg_beat  = parseInt(argv[3]);
  arg_grace = parseInt(argv[4]);
  
  printf("Ruler parameters\n");
  printf("----------------\n");
  printf("\n");
  
  printf("Slot : %ld\n", (long) arg_slot);
  printf("Gap  : %ld\n", (long) arg_gap);
  printf("\n");
  
  printf("Constructing ruler...\n\n");
  
  pr = ruler_new(arg_slot, arg_gap, 1);
  
  printf("Ruler: ");
  ruler_print(pr, stdout);
  printf("\n\n");
  
  printf("Test beat offset in subquanta : %ld\n", (long) arg_beat);
  printf("Test grace note index         : %ld\n", (long) arg_grace);
  printf("\n");
  
  printf("Computing grace note position...\n\n");
  
  result = ruler_pos(pr, arg_beat, arg_grace);
  
  printf("Test performance offset       : %ld\n", (long) result);
  printf("Test performance duration     : %ld\n", (long) ruler_dur(pr));
  
  ruler_shutdown();
  return EXIT_SUCCESS;
}
