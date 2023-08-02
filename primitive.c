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

/*
 * primitive_range_union function.
 */
primitive_range primitive_range_union(
    primitive_range a,
    primitive_range b) {
  
  return (a | b);
}

/*
 * primitive_range_intersect function.
 */
primitive_range primitive_range_intersect(
    primitive_range a,
    primitive_range b) {
  
  return (a & b);
}

/*
 * primitive_range_invert function.
 */
primitive_range primitive_range_invert(primitive_range r) {
  return (~r);
}

/*
 * primitive_range_art function.
 */
primitive_range primitive_range_art(
    primitive_range r,
    int32_t         art,
    int32_t         val,
    long            lnum) {
  
  primitive_range m = 0;
  
  if ((art < 0) || (art > 61)) {
    raiseErr(__LINE__,
      "Articulation index out of range on script line %ld",
      srcLine(lnum));
  }
  if ((val != 0) && (val != 1)) {
    raiseErr(__LINE__,
      "Invalid flag value on script line %ld",
      srcLine(lnum));
  }
  
  m = ((primitive_range) 1) << art;
  if (val) {
    r = r | m;
  } else {
    r = r & (~m);
  }
  
  return r;
}

/*
 * primitive_range_matrix function.
 */
primitive_range primitive_range_matrix(
    primitive_range r,
    int32_t         mat,
    int32_t         val,
    long            lnum) {
  
  primitive_range m = 0;
  
  if ((mat < 1) || (mat > 3)) {
    raiseErr(__LINE__,
      "Matrix selector out of range on script line %ld",
      srcLine(lnum));
  }
  if ((val != 0) && (val != 1)) {
    raiseErr(__LINE__,
      "Invalid flag value on script line %ld",
      srcLine(lnum));
  }
  
  m = ((primitive_range) mat) << 62;
  if (val) {
    r = r | m;
  } else {
    r = r & (~m);
  }
  
  return r;
}

/*
 * primitive_bitmap_union function.
 */
primitive_bitmap primitive_bitmap_union(
    primitive_bitmap a,
    primitive_bitmap b) {
  
  return (a | b);
}

/*
 * primitive_bitmap_intersection function.
 */
primitive_bitmap primitive_bitmap_intersect(
    primitive_bitmap a,
    primitive_bitmap b) {
  
  return (a & b);
}

/*
 * primitive_bitmap_invert function.
 */
primitive_bitmap primitive_bitmap_invert(primitive_bitmap r) {
  return (~r);
}

/*
 * primitive_bitmap_set function.
 */
primitive_bitmap primitive_bitmap_set(
    primitive_bitmap r,
    int32_t          ch,
    int32_t          val,
    long             lnum) {
  
  primitive_bitmap m = 0;
  
  if ((ch < 1) || (ch > 16)) {
    raiseErr(__LINE__, "Channel out of range on script line %ld",
      srcLine(lnum));
  }
  if ((val != 0) && (val != 1)) {
    raiseErr(__LINE__,
      "Invalid flag value on script line %ld",
      srcLine(lnum));
  }
  
  m = ((primitive_bitmap) 1) << (ch - 1);
  if (val) {
    r = r | m;
  } else {
    r = r & (~m);
  }
  
  return r;
}
