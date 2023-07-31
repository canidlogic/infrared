/*
 * blob.c
 * ======
 * 
 * Implementation of blob.h
 * 
 * See the header for further information.
 */

#include "blob.h"

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
 * Constants
 * =========
 */

/*
 * The initial capacity in bytes of the blob buffer.
 */
#define INIT_CAP INT32_C(256)

/*
 * Type declarations
 * =================
 */

/*
 * BLOB structure.  Prototype given in header.
 */
struct BLOB_TAG {
  
  /*
   * The next blob in the allocated blob chain, or NULL if this is the
   * last blob in the chain.
   * 
   * The blob chain allows the shutdown function to free all blobs.
   */
  BLOB *pNext;
  
  /*
   * The length of blob data in bytes.
   */
  int32_t blen;
  
  /*
   * The start of the blob data buffer.  The rest of the buffer extends
   * beyond the end of the structure.
   */
  uint8_t buf[1];
  
};

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static long srcLine(long lnum);

static BLOB *initBlob(int32_t *pCap);
static BLOB *appendBlob(BLOB *pb, int32_t *pCap, int c, long lnum);
static BLOB *finishBlob(BLOB *pb, int32_t *pCap);

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
 * Initialize a blob structure and capacity counter to get ready for
 * data.
 * 
 * Parameters:
 * 
 *   pCap - the variable that will be used to track buffer capacity
 * 
 * Return:
 * 
 *   the newly initialized blob
 */
static BLOB *initBlob(int32_t *pCap) {
  
  BLOB *pb = NULL;
  
  if (pCap == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  *pCap = INIT_CAP;
  
  pb = (BLOB *) calloc(1, ((size_t) (*pCap - 1)) + sizeof(BLOB));
  if (pb == NULL) {
    raiseErr(__LINE__, "Out of memory");
  }
  
  pb->pNext = NULL;
  pb->blen = 0;
  
  return pb;
}

/*
 * Append a data byte to an initialized blob structure.
 * 
 * Parameters:
 * 
 *   pb - the initialized blob
 * 
 *   pCap - the initialized buffer capacity variable
 * 
 *   c - the unsigned byte value to append
 * 
 *   lnum - the Shastina line number, for diagnostic messages
 * 
 * Return:
 * 
 *   the possibly re-allocated blob
 */
static BLOB *appendBlob(BLOB *pb, int32_t *pCap, int c, long lnum) {
  
  int32_t new_cap = 0;
  
  if ((pb == NULL) || (pCap == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  if ((c < 0) || (c > 255)) {
    raiseErr(__LINE__, NULL);
  }
  
  if (pb->blen >= *pCap) {
    if (pb->blen >= BLOB_MAXLEN) {
      raiseErr(__LINE__, "Blob too long on script line %ld",
        srcLine(lnum));
    }
    
    new_cap = *pCap * 2;
    if (new_cap > BLOB_MAXLEN) {
      new_cap = BLOB_MAXLEN;
    }
    
    pb = (BLOB *) realloc(pb, ((size_t) (new_cap - 1)) + sizeof(BLOB));
    if (pb == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }
    
    memset(
      &((pb->buf)[*pCap]),
      0,
      (size_t) (new_cap - *pCap));
    
    *pCap = new_cap;
  }
  
  (pb->buf)[pb->blen] = (uint8_t) c;
  (pb->blen)++;
  
  return pb;
}

/*
 * Finish the given blob in its current state.
 * 
 * The given buffer capacity variable can be forgotten about after this
 * call.
 * 
 * Parmeters:
 * 
 *   pb - the initialized blob
 * 
 *   pCap - the initialized buffer capacity variable
 * 
 * Return:
 * 
 *   the possibly re-allocated blob
 */
static BLOB *finishBlob(BLOB *pb, int32_t *pCap) {
  
  size_t sz = 0;
  
  if ((pb == NULL) || (pCap == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  if (*pCap > pb->blen) {
    if (pb->blen > 0) {
      sz = ((size_t) (pb->blen - 1)) + sizeof(BLOB);
    } else {
      sz = sizeof(BLOB);
    }
    
    pb = (BLOB *) realloc(pb, sz);
    if (pb == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }
  }
  
  return pb;
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
 * Pointers to the first and last blobs in the blob chain, or NULL for
 * both if no blobs allocated.
 */
static BLOB *m_pFirst = NULL;
static BLOB *m_pLast = NULL;

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * blob_fromHex function.
 */
BLOB *blob_fromHex(const char *pStr, long lnum) {
  
  BLOB *pb = NULL;
  int32_t cap = 0;
  int i = 0;
  int c = 0;
  int v = 0;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Blob module is shut down");
  }
  if (pStr == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  pb = initBlob(&cap);
  
  while (1) {
    for( ;
          (*pStr == ' ') || (*pStr == '\t') ||
          (*pStr == '\r') || (*pStr == '\n');
        pStr++);
    if (*pStr == 0) {
      break;
    }
    
    v = 0;
    for(i = 0; i < 2; i++) {
      c = *pStr;
      if ((c >= '0') && (c <= '9')) {
        c = c - '0';
        
      } else if ((c >= 'A') && (c <= 'F')) {
        c = (c - 'A') + 10;
        
      } else if ((c >= 'a') && (c <= 'f')) {
        c = (c - 'a') + 10;
        
      } else {
        raiseErr(__LINE__, "Invalid blob byte on script line %ld",
                  srcLine(lnum));
      }
      v = (v << 4) | c;
      pStr++;
    }
    
    pb = appendBlob(pb, &cap, v, lnum);
  }
  
  pb = finishBlob(pb, &cap);
  
  if (m_pLast == NULL) {
    m_pFirst = pb;
    m_pLast = pb;
    pb->pNext = NULL;
  } else {
    m_pLast->pNext = pb;
    pb->pNext = NULL;
    m_pLast = pb;
  }
  
  return pb;
}

/*
 * blob_concat function.
 */
BLOB *blob_concat(BLOB **ppList, int32_t list_len, long lnum) {
  
  int32_t i = 0;
  int32_t full_len = 0;
  int32_t fill = 0;
  size_t sz = 0;
  BLOB *pb = NULL;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Blob module is shut down");
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
    if (full_len > BLOB_MAXLEN) {
      raiseErr(__LINE__,
        "Concatenated blob length too large on script line %ld",
        srcLine(lnum));
    }
  }
  
  if (full_len > 0) {
    sz = ((size_t) (full_len - 1)) + sizeof(BLOB);
  } else {
    sz = sizeof(BLOB);
  }
  
  pb = (BLOB *) calloc(1, sz);
  if (pb == NULL) {
    raiseErr(__LINE__, "Out of memory");
  }
  
  pb->blen = full_len;
  
  fill = 0;
  for(i = 0; i < list_len; i++) {
    if ((ppList[i])->blen > 0) {
      memcpy(
        &((pb->buf)[fill]),
        (ppList[i])->buf,
        (size_t) (ppList[i])->blen);
      fill += (ppList[i])->blen;
    }
  }
  
  if (m_pLast == NULL) {
    raiseErr(__LINE__, NULL);
  } else {
    m_pLast->pNext = pb;
    pb->pNext = NULL;
    m_pLast = pb;
  }
  
  return pb;
}

/*
 * blob_slice function.
 */
BLOB *blob_slice(BLOB *pSrc, int32_t i, int32_t j, long lnum) {
  
  BLOB *pb = NULL;
  size_t sz = 0;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Blob module is shut down");
  }
  if (pSrc == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if ((i < 0) || (i > pSrc->blen)) {
    raiseErr(__LINE__,
      "Lower blob slice index out of range on script line %ld",
      srcLine(lnum));
  }
  if ((j < i) || (j > pSrc->blen)) {
    raiseErr(__LINE__,
      "Upper blob slice index out of range on script line %ld",
      srcLine(lnum));
  }
  
  sz = (size_t) (j - i);
  if (sz > 0) {
    sz = (sz - 1) + sizeof(BLOB);
  } else {
    sz = sizeof(BLOB);
  }
  
  pb = (BLOB *) calloc(1, sz);
  if (pb == NULL) {
    raiseErr(__LINE__, "Out of memory");
  }
  
  pb->blen = j - i;
  if (pb->blen > 0) {
    memcpy(
      pb->buf,
      &((pSrc->buf)[i]),
      (size_t) pb->blen);
  }
  
  if (m_pLast == NULL) {
    raiseErr(__LINE__, NULL);
  } else {
    m_pLast->pNext = pb;
    pb->pNext = NULL;
    m_pLast = pb;
  }
  
  return pb;
}

/*
 * blob_shutdown function.
 */
void blob_shutdown(void) {
  BLOB *pCur = NULL;
  BLOB *pNext = NULL;
  
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
 * blob_ptr function.
 */
const uint8_t *blob_ptr(BLOB *pb) {
  
  const uint8_t *pResult = NULL;
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Blob module is shut down");
  }
  if (pb == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if (pb->blen > 0) {
    pResult = pb->buf;
  } else {
    pResult = NULL;
  }
  
  return pResult;
}

/*
 * blob_len function.
 */
int32_t blob_len(BLOB *pb) {
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Blob module is shut down");
  }
  if (pb == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  return pb->blen;
}
