/*
 * test_pointer.c
 * ==============
 * 
 * Test program for the pointer module of Infrared.
 * 
 * Syntax
 * ------
 * 
 *   test_pointer [nmf] [sect] [offs] [g] [r_s] [r_g] [tilt] [m]
 * 
 * [nmf] is the path to an NMF file, which is used to determine the base
 * times of NMF sections.
 * 
 * The [sect] [offs] [g] [tilt] and [m] parameters are signed integers
 * that are used to set the section, quantum offset, grace note offset,
 * tilt, and moment part of the pointer.  [g] is zero for no grace note
 * offset, or negative for a grace note index.  [m] is -1 for
 * start-of-moment, 0 for middle-of-moment, or 1 for end-of-moment.
 * 
 * The [r_s] and [r_g] parameters are used to construct a ruler for
 * positioning grace notes, or use a hyphen for both for no ruler.
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Infrared modules:
 * 
 *   - diagnostic.c
 *   - pointer.c
 *   - ruler.c
 * 
 * Requires the following external libraries:
 * 
 *   - libnmf
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostic.h"
#include "pointer.h"
#include "ruler.h"

#include "nmf.h"

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
  
  const char *arg_path = NULL;
  
  int32_t arg_sect = 0;
  int32_t arg_offs = 0;
  int32_t arg_g = 0;
  
  int arg_has_ruler = 0;
  int32_t arg_slot = 0;
  int32_t arg_gap = 0;
  
  int32_t arg_tilt = 0;
  int32_t arg_m = 0;
  
  int32_t result = 0;
  int32_t x = 0;
  int32_t y = 0;
  int i = 0;
  
  RULER *pr = NULL;
  POINTER *pp = NULL;
  NMF_DATA *pd = NULL;
  
  diagnostic_startup(argc, argv, "test_pointer");
  
  if (argc != 9) {
    raiseErr(__LINE__, "Wrong number of program arguments");
  }
  
  arg_path = argv[1];
  
  arg_sect = parseInt(argv[2]);
  arg_offs = parseInt(argv[3]);
  arg_g    = parseInt(argv[4]);
  
  if ((strcmp(argv[5], "-") == 0) && (strcmp(argv[6], "-") == 0)) {
    arg_has_ruler = 0;
  
  } else if ((strcmp(argv[5], "-") == 0) || 
              (strcmp(argv[6], "-") == 0)) {
    raiseErr(__LINE__, "Invalid ruler arguments");
    
  } else {
    arg_has_ruler = 1;
    arg_slot = parseInt(argv[5]);
    arg_gap  = parseInt(argv[6]);
  }
  
  arg_tilt = parseInt(argv[7]);
  arg_m    = parseInt(argv[8]);
  
  printf("Loading NMF file...\n\n");
  pd = nmf_parse_path(arg_path);
  if (pd == NULL) {
    raiseErr(__LINE__, "Failed to load NMF file");
  }
  
  printf("Initializing pointer system...\n\n");
  pointer_init(pd);
  
  printf("Pointer parameters\n");
  printf("------------------\n");
  printf("\n");
  
  printf("Section     : %ld\n", (long) arg_sect);
  printf("Offset      : %ld\n", (long) arg_offs);
  printf("Grace index : %ld\n", (long) arg_g);
  if (arg_has_ruler) {
    printf("Ruler slot  : %ld\n", (long) arg_slot);
    printf("Ruler gap   : %ld\n", (long) arg_gap);
  } else {
    printf("Ruler slot  : -\n");
    printf("Ruler gap   : -\n");
  }
  printf("Tilt        : %ld\n", (long) arg_tilt);
  printf("Moment part : %ld\n", (long) arg_m);
  printf("\n");
  
  if (arg_has_ruler) {
    printf("Constructing ruler...\n\n");
    pr = ruler_new(arg_slot, arg_gap, 1);
  }
  
  printf("Constructing pointer...\n\n");
  
  pp = pointer_new();
  pointer_jump(pp, arg_sect, 2);
  pointer_seek(pp, arg_offs, 3);
  pointer_grace(pp, arg_g, pr, 4);
  pointer_tilt(pp, arg_tilt, 5);
  pointer_moment(pp, arg_m, 6);
  
  printf("Pointer: ");
  pointer_print(pp, stdout);
  printf("\n\n");
  
  printf("Computing pointer...\n\n");
  result = pointer_compute(pp, 7);
  
  printf("Computed moment offset: %ld\n", (long) result);
  
  printf("\n");
  
  printf("Testing pointer conversion...\n\n");
  
  for(x = INT32_MIN; x < INT32_MIN + 64; x++) {
    y = pointer_unpack(x, &i);
    if (pointer_pack(y, i) != x) {
      raiseErr(__LINE__, "Pointer test failed");
    }
  }
  
  for(x = -32; x < 33; x++) {
    y = pointer_unpack(x, &i);
    if (pointer_pack(y, i) != x) {
      raiseErr(__LINE__, "Pointer test failed");
    }
  }
  
  for(x = INT32_MAX; x > INT32_MAX - 64; x--) {
    y = pointer_unpack(x, &i);
    if (pointer_pack(y, i) != x) {
      raiseErr(__LINE__, "Pointer test failed");
    }
  }
  
  ruler_shutdown();
  pointer_shutdown();
  
  nmf_free(pd);
  pd = NULL;
  
  diagnostic_log("Test successful");
  return EXIT_SUCCESS;
}
