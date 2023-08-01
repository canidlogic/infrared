/*
 * ruler.c
 * =======
 * 
 * Implementation of ruler.h
 * 
 * See the header for further information.
 */

#include "ruler.h"

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
 * RULER structure.  Prototype given in header.
 */
struct RULER_TAG {
  
  /*
   * The next ruler in the allocated ruler chain, or NULL if this is the
   * last ruler in the chain.
   * 
   * The ruler chain allows the shutdown function to free all rulers.
   */
  RULER *pNext;
  
  /*
   * The slot duration in subquanta.
   * 
   * Greater than zero.
   */
  int32_t slot;
  
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
 * Pointers to the first and last rulers in the ruler chain, or NULL for
 * both if no rulers allocated.
 */
static RULER *m_pFirst = NULL;
static RULER *m_pLast = NULL;

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * ruler_new function.
 */
RULER *ruler_new(int32_t slot, int32_t gap, long lnum) {
  
  RULER *pr = NULL;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Ruler module is shut down");
  }
  
  if (slot < 1) {
    raiseErr(__LINE__,
      "Ruler slot out of range on script line %ld",
      srcLine(lnum));
  }
  
  if (gap > 0) {
    raiseErr(__LINE__,
      "Ruler gap out of range on script line %ld",
      srcLine(lnum));
  }
  
  if (slot + gap < 1) {
    raiseErr(__LINE__,
      "Ruler gap too large for slot on script line %ld",
      srcLine(lnum));
  }
  
  pr = (RULER *) calloc(1, sizeof(RULER));
  if (pr == NULL) {
    raiseErr(__LINE__, "Out of memory");
  }
  
  pr->slot = slot;
  pr->gap  = gap;
  
  if (m_pLast == NULL) {
    m_pFirst = pr;
    m_pLast = pr;
    pr->pNext = NULL;
  } else {
    m_pLast->pNext = pr;
    pr->pNext = NULL;
    m_pLast = pr;
  }
  
  return pr;
}

/*
 * ruler_shutdown function.
 */
void ruler_shutdown(void) {
  RULER *pCur = NULL;
  RULER *pNext = NULL;
  
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
 * ruler_pos function.
 */
int32_t ruler_pos(RULER *pr, int32_t beat, int32_t i) {
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Ruler module is shut down");
  }
  if (pr == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if (i >= 0) {
    raiseErr(__LINE__, NULL);
  }
  
  
  if (i >= INT32_MIN / pr->slot) {
    i *= pr->slot;
  } else {
    raiseErr(__LINE__, "Ruler overflow");
  }
  
  if (beat >= INT32_MIN - i) {
    i += beat;
  } else {
    raiseErr(__LINE__, "Ruler overflow");
  }
  
  return i;
}

/*
 * ruler_dur function.
 */
int32_t ruler_dur(RULER *pr) {
  
  int32_t result = 0;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Ruler module is shut down");
  }
  if (pr == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  return (pr->slot + pr->gap);
}
