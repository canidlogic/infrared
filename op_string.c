/*
 * op_string.c
 * ===========
 * 
 * Implementation of op_string.h
 * 
 * See the header for further information.  See the Infrared operations
 * documentation for specifications of the operations.
 */

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blob.h"
#include "core.h"
#include "diagnostic.h"
#include "main.h"
#include "text.h"

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

#define MAX_CONCAT (16384)

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

static void op_concat(void *pCustom, long lnum) {
  CORE_VARIANT cv;
  int32_t n = 0;
  int32_t i = 0;
  int is_blob = 0;
  TEXT **ppt = NULL;
  BLOB **ppb = NULL;
  
  /* Ignore parameter */
  (void) pCustom;
  
  /* Initialize structures */
  memset(&cv, 0, sizeof(CORE_VARIANT));
  
  /* Get the element count and verify that it is at least one and does
   * not exceed the maximum */
  n = core_pop_i(lnum);
  if (n < 1) {
    raiseErr(__LINE__,
      "Element count for concat must be at least one on line %ld",
      srcLine(lnum));
  }
  if (n > MAX_CONCAT) {
    raiseErr(__LINE__,
      "Element count for concat may be at most %ld on line %ld",
      (long) MAX_CONCAT,
      srcLine(lnum));
  }
  
  /* Pop the last element, check its type to determine if this is a blob
   * or text concat, and push the last element back on the stack */
  core_pop(&cv, lnum);
  if (cv.tcode == CORE_T_TEXT) {
    is_blob = 0;
    
  } else if (cv.tcode == CORE_T_BLOB) {
    is_blob = 1;
    
  } else {
    raiseErr(__LINE__, "Expecting blob or text on script line %ld",
      srcLine(lnum));
  }
  core_push(&cv, lnum);
  
  /* Allocate the concatenation array */
  if (is_blob) {
    ppb = (BLOB **) calloc((size_t) n, sizeof(BLOB *));
    if (ppb == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }
    
  } else {
    ppt = (TEXT **) calloc((size_t) n, sizeof(TEXT *));
    if (ppt == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }
  }
  
  /* Pop all the elements and add them to the array in reverse order */
  for(i = n - 1; i >= 0; i--) {
    if (is_blob) {
      ppb[i] = core_pop_b(lnum);
      
    } else {
      ppt[i] = core_pop_t(lnum);
    }
  }
  
  /* Push the concatenated object */
  if (is_blob) {
    core_push_b(blob_concat(ppb, n, lnum), lnum);
    
  } else {
    core_push_t(text_concat(ppt, n, lnum), lnum);
  }
  
  /* Free the concatenation arrays */
  if (ppt != NULL) {
    free(ppt);
    ppt = NULL;
  }
  if (ppb != NULL) {
    free(ppb);
    ppb = NULL;
  }
}

static void op_slice(void *pCustom, long lnum) {
  
  int32_t i = 0;
  int32_t j = 0;
  CORE_VARIANT cv;
  
  (void) pCustom;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  
  j = core_pop_i(lnum);
  i = core_pop_i(lnum);
  core_pop(&cv, lnum);
  
  if (cv.tcode == CORE_T_BLOB) {
    core_push_b(blob_slice(cv.v.pBlob, i, j, lnum), lnum);
    
  } else if (cv.tcode == CORE_T_TEXT) {
    core_push_t(text_slice(cv.v.pText, i, j, lnum), lnum);
    
  } else {
    raiseErr(__LINE__, "Expecting blob or text on script line %ld",
      srcLine(lnum));
  }
}

/*
 * Registration function
 * =====================
 */

void op_string_register(void) {
  main_op("concat", &op_concat, NULL);
  main_op("slice", &op_slice, NULL);
}
