/*
 * test_art.c
 * ==========
 * 
 * Test program for the articulation module of Infrared.
 * 
 * Syntax
 * ------
 * 
 *   test_art [scale_num] [scale_denom] [bumper] [gap] [test]
 * 
 * All parameters except the last are signed integers that are used to
 * construct an articulation object.
 * 
 * The [test] is an NMF duration greater than zero, measured in quanta,
 * which is used to test the duration transformation function.
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Infrared modules:
 * 
 *   - art.c
 *   - diagnostic.c
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "art.h"
#include "diagnostic.h"

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
  
  int32_t arg_scale_num = 0;
  int32_t arg_scale_denom = 0;
  int32_t arg_bumper = 0;
  int32_t arg_gap = 0;
  int32_t arg_test = 0;
  
  int32_t result = 0;
  
  ART *pa = NULL;
  
  diagnostic_startup(argc, argv, "test_art");
  
  if (argc != 6) {
    raiseErr(__LINE__, "Wrong number of program arguments");
  }
  
  arg_scale_num   = parseInt(argv[1]);
  arg_scale_denom = parseInt(argv[2]);
  arg_bumper      = parseInt(argv[3]);
  arg_gap         = parseInt(argv[4]);
  arg_test        = parseInt(argv[5]);
  
  printf("Articulation parameters\n");
  printf("-----------------------\n");
  printf("\n");
  
  printf("Scale numerator   : %ld\n", (long) arg_scale_num);
  printf("Scale denominator : %ld\n", (long) arg_scale_denom);
  printf("Bumper            : %ld\n", (long) arg_bumper);
  printf("Gap               : %ld\n", (long) arg_gap);
  printf("\n");
  
  printf("Constructing articulation...\n\n");
  
  pa = art_new(
        arg_scale_num,
        arg_scale_denom,
        arg_bumper,
        arg_gap,
        1);
  
  printf("Test input duration in quanta : %ld\n", (long) arg_test);
  printf("\n");
  
  printf("Transforming input duration...\n\n");
  
  result = art_transform(pa, arg_test);
  
  printf("Test performance subquanta    : %ld\n", (long) result);
  
  art_shutdown();
  return EXIT_SUCCESS;
}
