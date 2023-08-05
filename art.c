/*
 * art.c
 * =====
 * 
 * Implementation of art.h
 * 
 * See the header for further information.
 */

#include "art.h"

#include <limits.h>
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
 * Type declarations
 * =================
 */

/*
 * ART structure.  Prototype given in header.
 */
struct ART_TAG {
  
  /*
   * The next articulation in the allocated articulation chain, or NULL
   * if this is the last articulation in the chain.
   * 
   * The articulation chain allows the shutdown function to free all
   * articulations.
   */
  ART *pNext;
  
  /*
   * The numerator of the duration scaling factor, with an assumed
   * denominator of 8.
   * 
   * Range is 1 to 8, inclusive.
   */
  int scale;
  
  /*
   * The bumper duration in subquanta.
   * 
   * Zero or greater.
   */
  int32_t bumper;
  
  /*
   * The gap duration in subquanta.
   * 
   * Zero or negative.
   */
  int32_t gap;
  
};

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static long srcLine(long lnum);

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
 * Local data
 * ==========
 */

/*
 * Set to 1 if the module has been shut down.
 */
static int m_shutdown = 0;

/*
 * Pointers to the first and last articulations in the articulation
 * chain, or NULL for both if no articulations allocated.
 */
static ART *m_pFirst = NULL;
static ART *m_pLast = NULL;

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * art_new function.
 */
ART *art_new(
    int32_t scale_num,
    int32_t scale_denom,
    int32_t bumper,
    int32_t gap,
    long    lnum) {
  
  ART *pa = NULL;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Articulation module is shut down");
  }
  
  if ((scale_denom != 1) && (scale_denom != 2) &&
      (scale_denom != 4) && (scale_denom != 8)) {
    raiseErr(__LINE__,
      "Articulation scale denominator is not valid on script line %ld",
      srcLine(lnum));
  }
  
  if ((scale_num < 1) || (scale_num > scale_denom)) {
    raiseErr(__LINE__,
      "Articulation scale numerator out of range on script line %ld",
      srcLine(lnum));
  }
  
  if (bumper < 0) {
    raiseErr(__LINE__,
      "Articulation bumper out of range on script line %ld",
      srcLine(lnum));
  }
  
  if (gap > 0) {
    raiseErr(__LINE__,
      "Articulation gap out of range on script line %ld",
      srcLine(lnum));
  }
  
  pa = (ART *) calloc(1, sizeof(ART));
  if (pa == NULL) {
    raiseErr(__LINE__, "Out of memory");
  }
  
  while (scale_denom < 8) {
    scale_num *= 2;
    scale_denom *= 2;
  }
  
  pa->scale  = (int) scale_num;
  pa->bumper = bumper;
  pa->gap    = gap;
  
  if (m_pLast == NULL) {
    m_pFirst = pa;
    m_pLast = pa;
    pa->pNext = NULL;
  } else {
    m_pLast->pNext = pa;
    pa->pNext = NULL;
    m_pLast = pa;
  }
  
  return pa;
}

/*
 * art_shutdown function.
 */
void art_shutdown(void) {
  ART *pCur = NULL;
  ART *pNext = NULL;
  
  if (!m_shutdown) {
    m_shutdown = 1;
    pCur = m_pFirst;
    while (pCur != NULL) {
      pNext = pCur->pNext;
      free(pCur);
      pCur = pNext;
    }
    m_pFirst = NULL;
    m_pLast = NULL;
  }
}

/*
 * art_transform function.
 */
int32_t art_transform(ART *pa, int32_t dur) {
  
  int32_t result = 0;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Articulation module is shut down");
  }
  if (pa == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if (dur < 1) {
    raiseErr(__LINE__, NULL);
  }
  
  if (dur <= INT32_MAX / 8) {
    dur *= 8;
  }
  
  if (dur <= INT32_MAX / pa->scale) {
    result = dur * ((int32_t) pa->scale);
  } else {
    raiseErr(__LINE__, "Duration overflow");
  }
  
  result = result / 8;
  
  if (result < pa->bumper) {
    result = pa->bumper;
  }
  
  if (result > dur + pa->gap) {
    result = dur + pa->gap;
  }
  
  if (result < 1) {
    result = 1;
  }
  
  return result;
}
