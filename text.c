/*
 * text.c
 * ======
 * 
 * Implementation of text.h
 * 
 * See the header for further information.
 */

#include "text.h"

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
 * TEXT structure.  Prototype given in header.
 */
struct TEXT_TAG {
  
  /*
   * The next text in the allocated text chain, or NULL if this is the
   * last text in the chain.
   * 
   * The text chain allows the shutdown function to free all texts.
   */
  TEXT *pNext;
  
  /*
   * The length of text data in characters, excluding the terminating
   * nul.
   */
  int32_t blen;
  
  /*
   * The start of the text data buffer.  The rest of the buffer extends
   * beyond the end of the structure.  It will be nul terminated.
   */
  char buf[1];
  
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
 * Pointers to the first and last texts in the text chain, or NULL for
 * both if no texts allocated.
 */
static TEXT *m_pFirst = NULL;
static TEXT *m_pLast = NULL;

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * text_literal function.
 */
TEXT *text_literal(const char *pStr, long lnum) {
  
  TEXT *pt = NULL;
  size_t slen = 0;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Text module is shut down");
  }
  if (pStr == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  slen = strlen(pStr);
  if (slen > TEXT_MAXLEN) {
    raiseErr(__LINE__, "Text literal too long on script line %ld",
              srcLine(lnum));
  }
  
  pt = (TEXT *) calloc(1, slen + sizeof(TEXT));
  if (pt == NULL) {
    raiseErr(__LINE__, "Out of memory");
  }
  
  pt->blen = (int32_t) slen;
  memcpy(pt->buf, pStr, slen + 1);
  
  if (m_pLast == NULL) {
    m_pFirst = pt;
    m_pLast = pt;
    pt->pNext = NULL;
  } else {
    m_pLast->pNext = pt;
    pt->pNext = NULL;
    m_pLast = pt;
  }
  
  return pt;
}

/*
 * text_concat function.
 */
TEXT *text_concat(TEXT **ppList, int32_t list_len, long lnum) {
  
  int32_t i = 0;
  int32_t full_len = 0;
  int32_t fill = 0;
  TEXT *pt = NULL;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Text module is shut down");
  }
  if (list_len < 0) {
    raiseErr(__LINE__, NULL);
  }
  if (list_len > 0) {
    if (ppList == NULL) {
      raiseErr(__LINE__, NULL);
    }
  }
  
  full_len = 0;
  for(i = 0; i < list_len; i++) {
    if (ppList[i] == NULL) {
      raiseErr(__LINE__, NULL);
    }
    full_len += (ppList[i])->blen;
    if (full_len > TEXT_MAXLEN) {
      raiseErr(__LINE__,
        "Concatenated text length too large on script line %ld",
        srcLine(lnum));
    }
  }
  
  pt = (TEXT *) calloc(1, sizeof(TEXT) + ((size_t) full_len));
  if (pt == NULL) {
    raiseErr(__LINE__, "Out of memory");
  }
  
  pt->blen = full_len;
  
  fill = 0;
  for(i = 0; i < list_len; i++) {
    if ((ppList[i])->blen > 0) {
      memcpy(
        &((pt->buf)[fill]),
        (ppList[i])->buf,
        (size_t) (ppList[i])->blen);
      fill += (ppList[i])->blen;
    }
  }
  
  if (m_pLast == NULL) {
    raiseErr(__LINE__, NULL);
  } else {
    m_pLast->pNext = pt;
    pt->pNext = NULL;
    m_pLast = pt;
  }
  
  return pt;
}

/*
 * text_slice function.
 */
TEXT *text_slice(TEXT *pSrc, int32_t i, int32_t j, long lnum) {
  
  TEXT *pt = NULL;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Text module is shut down");
  }
  if (pSrc == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if ((i < 0) || (i > pSrc->blen)) {
    raiseErr(__LINE__,
      "Lower text slice index out of range on script line %ld",
      srcLine(lnum));
  }
  if ((j < i) || (j > pSrc->blen)) {
    raiseErr(__LINE__,
      "Upper text slice index out of range on script line %ld",
      srcLine(lnum));
  }
  
  pt = (TEXT *) calloc(1, sizeof(TEXT) + ((size_t) (j - i)));
  if (pt == NULL) {
    raiseErr(__LINE__, "Out of memory");
  }
  
  pt->blen = j - i;
  if (pt->blen > 0) {
    memcpy(
      pt->buf,
      &((pSrc->buf)[i]),
      (size_t) pt->blen);
  }
  
  if (m_pLast == NULL) {
    raiseErr(__LINE__, NULL);
  } else {
    m_pLast->pNext = pt;
    pt->pNext = NULL;
    m_pLast = pt;
  }
  
  return pt;
}

/*
 * text_shutdown function.
 */
void text_shutdown(void) {
  TEXT *pCur = NULL;
  TEXT *pNext = NULL;
  
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
 * text_ptr function.
 */
const char *text_ptr(TEXT *pt) {
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Text module is shut down");
  }
  if (pt == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  return pt->buf;
}

/*
 * text_len function.
 */
int32_t text_len(TEXT *pt) {
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Text module is shut down");
  }
  if (pt == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  return pt->blen;
}
