/*
 * primitive.c
 * ===========
 * 
 * Implementation of primitive.h
 * 
 * See the header for further information.
 */

#include "primitive.h"

#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

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
static long srcLine(long lnum);
static void checkInt(int32_t i);

/*
 * If the given line number is within valid range, return it as-is.  In
 * all other cases, return -1.
 * 
 * Parameters:
 * 
 *   lnum - the candidate line number
 * 
 * Return:
 * 
 *   the line number as-is if valid, else -1
 */
static long srcLine(long lnum) {
  if ((lnum < 1) || (lnum >= LONG_MAX)) {
    lnum = -1;
  }
  return lnum;
}

/*
 * Check that a given integer value is in primitive integer range.
 * 
 * Parameters:
 * 
 *   i - the integer to check
 */
static void checkInt(int32_t i) {
  if ((i < PRIMITIVE_INT_MIN) || (i > PRIMITIVE_INT_MAX)) {
    raiseErr(__LINE__, "Integer out of range");
  }
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * primitive_add function.
 */
int32_t primitive_add(int32_t a, int32_t b, long lnum) {
  
  int64_t result = 0;
  
  checkInt(a);
  checkInt(b);
  
  result = ((int64_t) a) + ((int64_t) b);
  if ((result < PRIMITIVE_INT_MIN) || (result > PRIMITIVE_INT_MAX)) {
    raiseErr(__LINE__,
      "Integer result out of range on script line %ld",
      srcLine(lnum));
  }
  
  return ((int32_t) result);
}

/*
 * primitive_sub function.
 */
int32_t primitive_sub(int32_t a, int32_t b, long lnum) {
  
  int64_t result = 0;
  
  checkInt(a);
  checkInt(b);
  
  result = ((int64_t) a) - ((int64_t) b);
  if ((result < PRIMITIVE_INT_MIN) || (result > PRIMITIVE_INT_MAX)) {
    raiseErr(__LINE__,
      "Integer result out of range on script line %ld",
      srcLine(lnum));
  }
  
  return ((int32_t) result);
}

/*
 * primitive_mul function.
 */
int32_t primitive_mul(int32_t a, int32_t b, long lnum) {
  
  int64_t result = 0;
  
  checkInt(a);
  checkInt(b);
  
  result = ((int64_t) a) * ((int64_t) b);
  if ((result < PRIMITIVE_INT_MIN) || (result > PRIMITIVE_INT_MAX)) {
    raiseErr(__LINE__,
      "Integer result out of range on script line %ld",
      srcLine(lnum));
  }
  
  return ((int32_t) result);
}

/*
 * primitive_div function.
 */
int32_t primitive_div(int32_t a, int32_t b, long lnum) {
  
  double f = 0.0;
  
  checkInt(a);
  checkInt(b);
  
  if (b == 0) {
    raiseErr(__LINE__, "Division by zero on script line %ld",
      srcLine(lnum));
  }
  
  f = floor(((double) a) / ((double) b));
  if (!isfinite(f)) {
    raiseErr(__LINE__,
      "Integer result out of range on script range %ld",
      srcLine(lnum));
  }
  
  if (!((f >= ((double) PRIMITIVE_INT_MIN))
          && (f <= ((double) PRIMITIVE_INT_MAX)))) {
    raiseErr(__LINE__,
      "Integer result out of range on script range %ld",
      srcLine(lnum));
  }
  
  return ((int32_t) f);
}

/*
 * primitive_neg function.
 */
int32_t primitive_neg(int32_t a) {
  
  checkInt(a);
  return (0 - a);
}
