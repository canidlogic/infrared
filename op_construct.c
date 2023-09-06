/*
 * op_construct.c
 * ==============
 * 
 * Implementation of op_construct.h
 * 
 * See the header for further information.  See the Infrared operations
 * documentation for specifications of the operations.
 */

#include "op_construct.h"

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "core.h"
#include "diagnostic.h"
#include "main.h"
#include "pointer.h"
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

static long srcLine(long lnum) {
  if ((lnum < 1) || (lnum >= LONG_MAX)) {
    lnum = -1;
  }
  return lnum;
}

/*
 * Operation functions
 * ===================
 */

static void op_art(void *pCustom, long lnum) {
  int32_t num = 0;
  int32_t denom = 0;
  int32_t bumper = 0;
  int32_t gap = 0;
  
  (void) pCustom;
  
  gap = core_pop_i(lnum);
  bumper = core_pop_i(lnum);
  denom = core_pop_i(lnum);
  num = core_pop_i(lnum);
  
  core_push_a(art_new(num, denom, bumper, gap, lnum), lnum);
}

static void op_ruler(void *pCustom, long lnum) {
  int32_t slot = 0;
  int32_t gap = 0;
  
  (void) pCustom;
  
  gap = core_pop_i(lnum);
  slot = core_pop_i(lnum);
  
  core_push_r(ruler_new(slot, gap, lnum), lnum);
}

static void op_ptr(void *pCustom, long lnum) {
  (void) pCustom;
  
  core_push_p(pointer_new(), lnum);
}

/*
 * Registration function
 * =====================
 */

void op_construct_register(void) {
  main_op("art", &op_art, NULL);
  main_op("ruler", &op_ruler, NULL);
  main_op("ptr", &op_ptr, NULL);
}
