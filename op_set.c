/*
 * op_set.c
 * ========
 * 
 * Implementation of op_set.h
 * 
 * See the header for further information.  See the Infrared operations
 * documentation for specifications of the operations.
 */

#include "op_set.h"

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "diagnostic.h"
#include "main.h"
#include "set.h"

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
 * Constants
 * =========
 */

#define OP_ALL (1)
#define OP_NONE (2)
#define OP_INVERT (3)

#define OP_INCLUDE (4)
#define OP_EXCLUDE (5)

#define OP_UNION (6)
#define OP_INTERSECT (7)
#define OP_EXCEPT (8)

/*
 * Local data
 * ==========
 */

static int i_all = OP_ALL;
static int i_none = OP_NONE;
static int i_invert = OP_INVERT;

static int i_include = OP_INCLUDE;
static int i_exclude = OP_EXCLUDE;

static int i_union = OP_UNION;
static int i_intersect = OP_INTERSECT;
static int i_except = OP_EXCEPT;

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

static void op_begin_set(void *pCustom, long lnum) {
  (void) pCustom;
  
  set_begin(lnum);
}

static void op_end_set(void *pCustom, long lnum) {
  (void) pCustom;
  
  core_push_s(set_end(lnum), lnum);
}

/*
 * pCustom points to an int which is either OP_ALL, OP_NONE, or
 * OP_INVERT.
 */
static void op_basic(void *pCustom, long lnum) {
  int *pt = NULL;
  
  if (pCustom == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  pt = (int *) pCustom;
  
  if (*pt == OP_ALL) {
    set_all(lnum);
    
  } else if (*pt == OP_NONE) {
    set_none(lnum);
    
  } else if (*pt == OP_INVERT) {
    set_invert(lnum);
    
  } else {
    raiseErr(__LINE__, NULL);
  }
}

/*
 * pCustom points to an int which is either OP_INCLUDE or OP_EXCLUDE.
 * This performs the closed range inclusions/exclusions.
 */
static void op_closed(void *pCustom, long lnum) {
  int *pt = NULL;
  int32_t a = 0;
  int32_t b = 0;
  
  if (pCustom == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  pt = (int *) pCustom;
  b = core_pop_i(lnum);
  a = core_pop_i(lnum);
  
  if (*pt == OP_INCLUDE) {
    set_rclose(a, b, 0, lnum);
    
  } else if (*pt == OP_EXCLUDE) {
    set_rclose(a, b, 1, lnum);
    
  } else {
    raiseErr(__LINE__, NULL);
  }
}

/*
 * pCustom points to an int which is either OP_INCLUDE or OP_EXCLUDE.
 * This performs the open range inclusions/exclusions.
 */
static void op_open(void *pCustom, long lnum) {
  int *pt = NULL;
  int32_t a = 0;
  
  if (pCustom == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  pt = (int *) pCustom;
  a = core_pop_i(lnum);
  
  if (*pt == OP_INCLUDE) {
    set_ropen(a, 0, lnum);
    
  } else if (*pt == OP_EXCLUDE) {
    set_ropen(a, 1, lnum);
    
  } else {
    raiseErr(__LINE__, NULL);
  }
}

/*
 * pCustom points to an int which is either OP_UNION, OP_INTERSECT, or
 * OP_EXCEPT.
 */
static void op_binary(void *pCustom, long lnum) {
  int *pt = NULL;
  SET *ps = NULL;
  
  if (pCustom == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  pt = (int *) pCustom;
  ps = core_pop_s(lnum);
  
  if (*pt == OP_UNION) {
    set_union(ps, lnum);
    
  } else if (*pt == OP_INTERSECT) {
    set_intersect(ps, lnum);
    
  } else if (*pt == OP_EXCEPT) {
    set_except(ps, lnum);
    
  } else {
    raiseErr(__LINE__, NULL);
  }
}

/*
 * Registration function
 * =====================
 */

void op_set_register(void) {
  main_op("begin_set", &op_begin_set, NULL);
  main_op("end_set", &op_end_set, NULL);
  
  main_op("all", &op_basic, &i_all);
  main_op("none", &op_basic, &i_none);
  main_op("invert", &op_basic, &i_invert);
  
  main_op("include", &op_closed, &i_include);
  main_op("exclude", &op_closed, &i_exclude);
  
  main_op("include_from", &op_open, &i_include);
  main_op("exclude_from", &op_open, &i_exclude);
  
  main_op("union", &op_binary, &i_union);
  main_op("intersect", &op_binary, &i_intersect);
  main_op("except", &op_binary, &i_except);
}
