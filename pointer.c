/*
 * pointer.c
 * =========
 * 
 * Implementation of pointer.h
 * 
 * See the header for further information.
 */

#include "pointer.h"

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
 * POINTER structure.  Prototype given in header.
 */
struct POINTER_TAG {
  
  /*
   * The next pointer in the allocated pointer chain, or NULL if this is
   * the last pointer in the chain.
   * 
   * The pointer chain allows the shutdown function to free all
   * pointers.
   */
  POINTER *pNext;
  
  /*
   * Non-zero if this is a header pointer; else, zero.
   */
  int head;
  
  /*
   * The NMF section this pointer is relative to.
   * 
   * Only used if head is zero.  Must be zero or greater.
   */
  int32_t sect;
  
  /*
   * The quantum offset from the start of the NMF section.
   * 
   * Only used if head is zero.  May have any value, including negative.
   */
  int32_t offs;
  
  /*
   * The unmeasured grace note offset from the beat, or zero if not an
   * unmeasured grace note.
   * 
   * Only used if head is zero.  Must be zero or negative.
   */
  int32_t g;
  
  /*
   * The ruler used for locating unmeasured grace notes.
   * 
   * Only used if head is zero AND g is negative.  Otherwise, ignored
   * and set to NULL.
   */
  RULER *gr;
  
  /*
   * The tilt is a subquantum offset to apply to the position.
   * 
   * Only used if head is zero.  May have any value, including negative.
   */
  int32_t tilt;
  
  /*
   * The moment part:  -1 for start-of-moment, 0 for middle-of-moment,
   * or 1 for end-of-moment.
   * 
   * Only used if head is zero.
   */
  int m;
  
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
 * Once the module is initialized, m_pd holds the parsed NMF data.
 * 
 * NULL if the module is not initialized or if the module is shut down.
 */
static NMF_DATA *m_pd = NULL;

/*
 * Set to 1 if the module has been shut down.
 */
static int m_shutdown = 0;

/*
 * Pointers to the first and last pointers in the pointer chain, or NULL
 * for both if no pointers allocated.
 */
static POINTER *m_pFirst = NULL;
static POINTER *m_pLast = NULL;

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * pointer_init function.
 */
void pointer_init(NMF_DATA *pd) {
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Pointer module is shut down");
  }
  if (m_pd != NULL) {
    raiseErr(__LINE__, "Pointer module already initialized");
  }
  
  if (pd == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if (nmf_basis(pd) != NMF_BASIS_Q96) {
    raiseErr(__LINE__, "Input NMF has wrong quantum basis");
  }
  
  m_pd = pd;
}

/*
 * pointer_new function.
 */
POINTER *pointer_new(void) {
  
  POINTER *pp = NULL;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Pointer module is shut down");
  }
  if (m_pd == NULL) {
    raiseErr(__LINE__, "Pointer module not initialized");
  }
  
  pp = (POINTER *) calloc(1, sizeof(POINTER));
  if (pp == NULL) {
    raiseErr(__LINE__, "Out of memory");
  }
  
  pp->head = 1;
  pp->sect = 0;
  pp->offs = 0;
  pp->g    = 0;
  pp->gr   = NULL;
  pp->tilt = 0;
  pp->m    = 0;
  
  if (m_pLast == NULL) {
    m_pFirst = pp;
    m_pLast = pp;
    pp->pNext = NULL;
  } else {
    m_pLast->pNext = pp;
    pp->pNext = NULL;
    m_pLast = pp;
  }
  
  return pp;
}

/*
 * pointer_shutdown function.
 */
void pointer_shutdown(void) {
  POINTER *pCur = NULL;
  POINTER *pNext = NULL;
  
  if (!m_shutdown) {
    m_shutdown = 1;
    m_pd = NULL;
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
 * pointer_reset function.
 */
void pointer_reset(POINTER *pp) {
  if (m_shutdown) {
    raiseErr(__LINE__, "Pointer module is shut down");
  }
  if (m_pd == NULL) {
    raiseErr(__LINE__, "Pointer module not initialized");
  }
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  pp->head = 1;
  pp->sect = 0;
  pp->offs = 0;
  pp->g    = 0;
  pp->gr   = NULL;
  pp->tilt = 0;
  pp->m    = 0;
}

/*
 * pointer_jump function.
 */
void pointer_jump(POINTER *pp, int32_t sect, long lnum) {
  if (m_shutdown) {
    raiseErr(__LINE__, "Pointer module is shut down");
  }
  if (m_pd == NULL) {
    raiseErr(__LINE__, "Pointer module not initialized");
  }
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if (sect < 0) {
    raiseErr(__LINE__, "Invalid pointer section on script line %ld",
              srcLine(lnum));
  }
  
  if (pp->head) {
    pp->head = 0;
    pp->m = 0;
  }
  
  pp->sect = sect;
  pp->offs = 0;
  pp->g    = 0;
  pp->gr   = NULL;
  pp->tilt = 0;
}

/*
 * pointer_seek function.
 */
void pointer_seek(POINTER *pp, int32_t offs, long lnum) {
  if (m_shutdown) {
    raiseErr(__LINE__, "Pointer module is shut down");
  }
  if (m_pd == NULL) {
    raiseErr(__LINE__, "Pointer module not initialized");
  }
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if (pp->head) {
    raiseErr(__LINE__, "Can't seek a header pointer on script line %ld",
              srcLine(lnum));
  }
  
  pp->offs = offs;
  pp->g    = 0;
  pp->gr   = NULL;
  pp->tilt = 0;
}

/*
 * pointer_advance function.
 */
void pointer_advance(POINTER *pp, int32_t rel, long lnum) {
  if (m_shutdown) {
    raiseErr(__LINE__, "Pointer module is shut down");
  }
  if (m_pd == NULL) {
    raiseErr(__LINE__, "Pointer module not initialized");
  }
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if (pp->head) {
    raiseErr(__LINE__,
              "Can't advance a header pointer on script line %ld",
              srcLine(lnum));
  }
  
  if (rel >= 0) {
    if (pp->offs <= INT32_MAX - rel) {
      pointer_seek(pp, pp->offs + rel, lnum);
    } else {
      raiseErr(__LINE__,
                "Pointer overflow during advance on script line %ld",
                srcLine(lnum));
    }
    
  } else if (rel < 0) {
    if (pp->offs >= INT32_MIN - rel) {
      pointer_seek(pp, pp->offs + rel, lnum);
    } else {
      raiseErr(__LINE__,
                "Pointer overflow during advance on script line %ld",
                srcLine(lnum));
    }
  
  } else {
    raiseErr(__LINE__, NULL);
  }
}

/*
 * pointer_grace function.
 */
void pointer_grace(POINTER *pp, int32_t g, RULER *pr, long lnum) {
  if (m_shutdown) {
    raiseErr(__LINE__, "Pointer module is shut down");
  }
  if (m_pd == NULL) {
    raiseErr(__LINE__, "Pointer module not initialized");
  }
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if (g > 0) {
    raiseErr(__LINE__, "Invalid grace note offset on script line %ld",
              srcLine(lnum));
  }
  
  if (g < 0) {
    if (pr == NULL) {
      raiseErr(__LINE__, "Missing ruler parameter on script line %ld",
                srcLine(lnum));
    }
  } else if (g == 0) {
    pr = NULL;
  
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  if (pp->head) {
    raiseErr(__LINE__,
              "Can't grace-offset a header pointer on script line %ld",
              srcLine(lnum));
  }
  
  pp->g    = g;
  pp->gr   = pr;
  pp->tilt = 0;
}

/*
 * pointer_tilt function.
 */
void pointer_tilt(POINTER *pp, int32_t tilt, long lnum) {
  if (m_shutdown) {
    raiseErr(__LINE__, "Pointer module is shut down");
  }
  if (m_pd == NULL) {
    raiseErr(__LINE__, "Pointer module not initialized");
  }
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if (pp->head) {
    raiseErr(__LINE__, "Can't tilt a header pointer on script line %ld",
              srcLine(lnum));
  }
  
  pp->tilt = tilt;
}

/*
 * pointer_moment function.
 */
void pointer_moment(POINTER *pp, int32_t m, long lnum) {
  if (m_shutdown) {
    raiseErr(__LINE__, "Pointer module is shut down");
  }
  if (m_pd == NULL) {
    raiseErr(__LINE__, "Pointer module not initialized");
  }
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if ((m < -1) || (m > 1)) {
    raiseErr(__LINE__, "Invalid moment part on script line %ld",
              srcLine(lnum));
  }
  
  if (pp->head) {
    raiseErr(__LINE__,
          "Can't adjust moment on a header pointer on script line %ld",
              srcLine(lnum));
  }
  
  pp->m = (int) m;
}

/*
 * pointer_isHeader function.
 */
int pointer_isHeader(POINTER *pp) {
  if (m_shutdown) {
    raiseErr(__LINE__, "Pointer module is shut down");
  }
  if (m_pd == NULL) {
    raiseErr(__LINE__, "Pointer module not initialized");
  }
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  return pp->head;
}

/*
 * pointer_compute function.
 */
int32_t pointer_compute(POINTER *pp, long lnum) {
  
  int32_t result = 0;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Pointer module is shut down");
  }
  if (m_pd == NULL) {
    raiseErr(__LINE__, "Pointer module not initialized");
  }
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if (pp->head) {
    raiseErr(__LINE__, "Can't compute a header pointer");
  }
  
  if (pp->sect >= nmf_sections(m_pd)) {
    raiseErr(__LINE__,
              "Pointer section out of NMF range on script line %ld",
              srcLine(lnum));
  }
  
  /* Section */
  result = nmf_offset(m_pd, pp->sect);
  
  /* Quantum offset */
  if (pp->offs >= 0) {
    if (result <= INT32_MAX - pp->offs) {
      result += pp->offs;
    } else {
      raiseErr(__LINE__,
                "Overflow while computing pointer on script line %ld",
                srcLine(lnum));
    }
    
  } else if (pp->offs < 0) {
    if (result >= INT32_MIN - pp->offs) {
      result += pp->offs;
    } else {
      raiseErr(__LINE__,
                "Overflow while computing pointer on script line %ld",
                srcLine(lnum));
    }
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  /* Convert to subquanta */
  if (result >= 0) {
    if (result <= INT32_MAX / 8) {
      result *= 8;
    } else {
      raiseErr(__LINE__,
                "Overflow while computing pointer on script line %ld",
                srcLine(lnum));
    }
    
  } else if (result < 0) {
    if (result >= INT32_MIN / 8) {
      result *= 8;
    } else {
      raiseErr(__LINE__,
                "Overflow while computing pointer on script line %ld",
                srcLine(lnum));
    }
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  /* Grace note index */
  if (pp->g < 0) {
    result = ruler_pos(pp->gr, result, pp->g);
  }
  
  /* Tilt */
  if (pp->tilt >= 0) {
    if (result <= INT32_MAX - pp->tilt) {
      result += pp->tilt;
    } else {
      raiseErr(__LINE__,
                "Overflow while computing pointer on script line %ld",
                srcLine(lnum));
    }
    
  } else if (pp->tilt < 0) {
    if (result >= INT32_MIN - pp->tilt) {
      result += pp->tilt;
    } else {
      raiseErr(__LINE__,
                "Overflow while computing pointer on script line %ld",
                srcLine(lnum));
    }
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  /* Convert to moment offset */
  if (result >= 0) {
    if (result <= INT32_MAX / 3) {
      result *= 3;
    } else {
      raiseErr(__LINE__,
                "Overflow while computing pointer on script line %ld",
                srcLine(lnum));
    }
    
  } else if (result < 0) {
    if (result >= INT32_MIN / 3) {
      result *= 3;
    } else {
      raiseErr(__LINE__,
                "Overflow while computing pointer on script line %ld",
                srcLine(lnum));
    }
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  /* Moment part */
  if (result <= INT32_MAX - ((int32_t) (pp->m + 1))) {
    result += ((int32_t) (pp->m + 1));
  } else {
    raiseErr(__LINE__,
              "Overflow while computing pointer on script line %ld",
              srcLine(lnum));
  }
  
  return result;
}

/*
 * pointer_print function.
 */
void pointer_print(POINTER *pp, FILE *pOut) {
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Pointer module is shut down");
  }
  if (m_pd == NULL) {
    raiseErr(__LINE__, "Pointer module not initialized");
  }
  if ((pp == NULL) || (pOut == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  if (pp->head) {
    fprintf(pOut, "<header>");
    
  } else {
    fprintf(pOut, "(%ld,%ld,", (long) pp->sect, (long) pp->offs);
    if (pp->g < 0) {
      fprintf(pOut, "%ld:", (long) pp->g);
      ruler_print(pp->gr, pOut);
      fprintf(pOut, ",");
    } else {
      fprintf(pOut, ".,");
    }
    fprintf(pOut, "%ld,", (long) pp->tilt);
    if (pp->m < 0) {
      fprintf(pOut, "start)");
    } else if (pp->m == 0) {
      fprintf(pOut, "mid)");
    } else if (pp->m > 0) {
      fprintf(pOut, "end)");
    } else {
      raiseErr(__LINE__, NULL);
    }
  }
}

/*
 * pointer_unpack function.
 */
int32_t pointer_unpack(int32_t m, int *pPart) {
  
  int32_t s = 0;
  int32_t p = 0;
  
  if (m >= 0) {
    s = m / 3;
    p = m % 3;
    
  } else if (m <= INT32_MIN) {
    s = -715827883;
    p = 1;
  
  } else if ((m < 0) && (m > INT32_MIN)) {
    s = (0 - ((0 - m) / 3)) - 1;
    p = 3 - ((0 - m) % 3);
    if (p >= 3) {
      p = 0;
      s++;
    }
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  if (pPart != NULL) {
    *pPart = (int) p;
  }
  
  return s;
}

/*
 * pointer_pack function.
 */
int32_t pointer_pack(int32_t s, int p) {
  
  int32_t result = 0;

  if ((p < 0) || (p > 2)) {
    raiseErr(__LINE__, NULL);
  }
  
  if ((s > -715827883) && (s < 715827882))  {
    result = s * 3 + ((int32_t) p);
  
  } else if (s == 715827882) {
    if (p == 0) {
      result = INT32_MAX - 1;
    
    } else if (p == 1) {
      result = INT32_MAX;
      
    } else if (p == 2) {
      raiseErr(__LINE__, "Overflow while computing moment offset");
      
    } else {
      raiseErr(__LINE__, NULL);
    }
    
  } else if (s == -715827883) {
    if (p == 0) {
      raiseErr(__LINE__, "Overflow while computing moment offset");
      
    } else if (p == 1) {
      result = INT32_MIN;
      
    } else if (p == 2) {
      result = INT32_MIN + 1;
      
    } else {
      raiseErr(__LINE__, NULL);
    }
    
  } else if (s > 715827882) {
    raiseErr(__LINE__, "Overflow while computing moment offset");
    
  } else if (s < -715827883) {
    raiseErr(__LINE__, "Overflow while computing moment offset");
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  return result;
}
