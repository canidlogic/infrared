/*
 * set.c
 * =====
 * 
 * Implementation of set.h
 * 
 * See the header for further information.
 */

#include "set.h"

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
 * The maximum number of entries in a set object's table.
 */
#define SET_MAX_TABLE (16384)

/*
 * The initial and maximum capacity of the accumulator, as a count of
 * range objects.
 */
#define ACC_INIT_CAP (32)
#define ACC_MAX_CAP (16384)

/*
 * Type declarations
 * =================
 */

/*
 * SET structure.  Prototype given in header.
 */
struct SET_TAG {
  
  /*
   * The next set in the allocated set chain, or NULL if this is the
   * last set in the chain.
   * 
   * The set chain allows the shutdown function to free all sets.
   */
  SET *pNext;
  
  /*
   * The number of entries in the set table.
   * 
   * Must be in range 0 to SET_MAX_TABLE inclusive.
   */
  int32_t len;
  
  /*
   * The set table.
   * 
   * This extends beyond the end of the structure.  The number of
   * entries is given by the len field.
   * 
   * If the table is empty, then the set is an empty set.
   * 
   * Otherwise, the table consists of a sequence of one or more spans.
   * 
   * Each span starts at a integer value that is unique within the set
   * table.  The spans are organized in ascending order of integer
   * value.
   * 
   * There are two types of spans.  A closed span only includes its
   * integer value and nothing else.  An open span includes its integer
   * value and any integer values greater than that up to but excluding
   * the integer value of the next span.  If an open span is the last
   * span in the table, it includes all remaining integer values in the
   * integer range.
   * 
   * Each span is encoded as a single integer value in the table.
   * Closed spans have their integer value as-is, which will always be
   * zero or greater.  Open spans encode their integer value i with the
   * formula ((0 - i) - 1).
   * 
   * The integers in this table are sorted according to their decoded
   * span integer value, not their encoded integer values.
   */
  int32_t table[1];
  
};

/*
 * RANGE structure definition.
 * 
 * This is only used by the accumulator when defining a new set object.
 * The completed set objects use a different method of encoding ranges;
 * see the SET object definition for further information.
 * 
 * The RANGE fields indicate the lower and upper boundaries of the
 * range.  These are inclusive boundaries.
 * 
 * lo must be greater than or equal to zero and hi must be greater than
 * or equal to lo.
 * 
 * For the scanRange() function only, hi may have the special value -1
 * to indicate an open range with only a lower boundary.  This special
 * value is never allowed in the accumulator, however.
 */
typedef struct {
  int32_t lo;
  int32_t hi;
} RANGE;

/*
 * Local data
 * ==========
 */

/*
 * Set to 1 if the module has been shut down.
 */
static int m_shutdown = 0;

/*
 * Pointers to the first and last sets in the set chain, or NULL for 
 * both if no sets allocated.
 */
static SET *m_pFirst = NULL;
static SET *m_pLast = NULL;

/*
 * The current state of the accumulator.
 * 
 * Zero means no open set definition.  1 means a positive set is open in
 * the definition.  -1 means a negative set is open in the definition.
 */
static int m_state = 0;

/*
 * The accumulator register.
 * 
 * This is an array of RANGE objects, sorted in ascending order of
 * integer values covered by the ranges.  No ranges in the array may
 * overlap or be immediately adjacent to each other.
 * 
 * m_state determines whether the accumulator register is in range or if
 * there is a positive or negative set loaded.  Positive set means that
 * the range objects indicate all integer values in the set.  Negative
 * set means the range objects indicate all integer values not in the
 * set.
 * 
 * m_acc_cap is the total number of allocated range objects in the
 * array.  m_acc_len is the number of range objects actually in use in
 * the array.  m_acc is the pointer to the dynamically allocated array,
 * which is NULL if m_acc_cap is zero.
 */
static int32_t m_acc_cap = 0;
static int32_t m_acc_len = 0;
static RANGE *m_acc = NULL;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static long srcLine(long lnum);

static void accReset(void);
static void accInsert(int32_t i, const RANGE *pr, long lnum);
static void accReplace(int32_t i, const RANGE *pr);
static void accDelete(int32_t i);

static int32_t encodeEntry(int32_t v, int o);
static int32_t decodeEntry(int32_t e, int *po);
static int scanRange(const SET *ps, int32_t *pi, RANGE *pr);

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
 * Reset the accumulator back to an empty state.
 * 
 * The dynamic array in m_acc is allocated or resized to the initial
 * capacity if necessary and cleared to zero.  The state is then reset
 * so that the array is empty.
 */
static void accReset(void) {
  
  if (m_acc != NULL) {
    if (m_acc_cap != ACC_INIT_CAP) {
      m_acc = (RANGE *) realloc(m_acc,
                          sizeof(RANGE) * ((size_t) ACC_INIT_CAP));
      if (m_acc == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      memset(m_acc, 0, sizeof(RANGE) * ((size_t) ACC_INIT_CAP));
    }
    
  } else {
    m_acc = (RANGE *) calloc((size_t) ACC_INIT_CAP, sizeof(RANGE));
    if (m_acc == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }  
  }
  
  m_acc_len = 0;
  m_acc_cap = ACC_INIT_CAP;
}

/*
 * Insert a new range into the accumulator array.
 * 
 * m_state must not be zero.  (The accumulator must be loaded with
 * something.)
 * 
 * If m_acc_cap is zero, accReset() will be automatically called to
 * initialize the accumulator array.
 * 
 * pr is the range to insert.  The lower boundary must be zero or
 * greater and the upper boundary must be greater than or equal to the
 * lower boundary.
 * 
 * i is the index of the range in the array to insert the new range
 * before.  It may also have the special value -1, which means to append
 * the new range on the end of the array.  If i is not -1, then it must
 * be greater than or equal to zero and less than m_acc_len.
 * 
 * If i is -1 and the accumulator array is not empty, then the low
 * boundary of the given range must be at least two greater than the
 * upper boundary of the last range in the array.
 * 
 * If i is not -1, then the upper boundary of the given range must be at
 * least two less than the lower boundary of range index i.  Also, if i
 * is greater than zero, then the lower boundary of the given range must
 * be at least two greater than the upper boundary of range index
 * (i - 1).
 * 
 * The accumulator array will be expanded in capacity if necessary.  An
 * error occurs if the number of ranges in the array will exceed
 * ACC_MAX_CAP.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * reporting an out-of-capacity error if necessary.
 * 
 * Paramerters:
 * 
 *   i - the index of the range object to insert the new range before,
 *   or -1 to append the new range to the end of the array
 * 
 *   pr - the new range object, which will be copied into the array
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
static void accInsert(int32_t i, const RANGE *pr, long lnum) {
  
  int32_t new_cap = 0;
  int32_t j = 0;
  
  /* Check state and parameters */
  if (m_state == 0) {
    raiseErr(__LINE__, NULL);
  }
  if ((i < -1) || (i >= m_acc_len)) {
    raiseErr(__LINE__, NULL);
  }
  if (pr == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if ((pr->lo < 0) || (pr->hi < pr->lo)) {
    raiseErr(__LINE__, NULL);
  }

  /* Reset if necessary */
  if (m_acc_cap < 1) {
    accReset();
  }

  /* Check range fits in position */
  if ((i == -1) && (m_acc_len > 0)) {
    if (pr->lo - 2 < (m_acc[m_acc_len - 1]).hi) {
      raiseErr(__LINE__, NULL);
    }
  
  } else if (i != -1) {
    if ((m_acc[i]).lo - 2 < pr->hi) {
      raiseErr(__LINE__, NULL);
    }
    if (i > 0) {
      if (pr->lo - 2 < (m_acc[i - 1]).hi) {
        raiseErr(__LINE__, NULL);
      }
    }
  }

  /* Expand capacity if necessary */
  if (m_acc_len >= m_acc_cap) {
    if (m_acc_len >= ACC_MAX_CAP) {
      raiseErr(__LINE__, "Set is too complex on script line %ld",
                srcLine(lnum));
    }
    
    new_cap = m_acc_cap * 2;
    if (new_cap > ACC_MAX_CAP) {
      new_cap = ACC_MAX_CAP;
    }
    
    m_acc = (RANGE *) realloc(m_acc,
                        ((size_t) new_cap) * sizeof(RANGE));
    if (m_acc == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }
    
    memset(
          &(m_acc[m_acc_cap]),
          0,
          ((size_t) (new_cap - m_acc_cap)) * sizeof(RANGE));
    
    m_acc_cap = new_cap;
  }
 
  /* Shift elements to make room for new element if necessary */
  if (i >= 0) {
    for(j = m_acc_len - 1; j >= i; j--) {
      memcpy(
        &(m_acc[j + 1]),
        &(m_acc[j]),
        sizeof(RANGE));
    }
  }

  /* Insert new element */
  if (i < 0) {
    i = m_acc_len;
  }

  memcpy(
    &(m_acc[i]),
    pr,
    sizeof(RANGE));
  
  m_acc_len++;
}

/*
 * Replace a range in the accumulator array with a new range.
 * 
 * m_state must not be zero.  (The accumulator must be loaded with
 * something.)
 * 
 * i is the index of the range to replace in the array.  It must be
 * greater than or equal to zero and less than m_acc_len.
 * 
 * pr is the range to insert.  The lower boundary must be zero or
 * greater and the upper boundary must be greater than or equal to the
 * lower boundary.
 * 
 * If i is greater than zero, then the lower boundary of the new range
 * must be at least two greater than the upper boundary of the range
 * index (i - 1).
 * 
 * If i is at least two less than m_acc_len, then the upper boundary of
 * the new range must be at least two less than the lower boundary of
 * the range index (i + 1).
 * 
 * The given range is then copied into the accumulator array,
 * overwriting the range entry at index i.
 * 
 * Parameters:
 * 
 *   i - the index of the range object to replace
 * 
 *   pr - the new range to overwrite the existing range
 */
static void accReplace(int32_t i, const RANGE *pr) {
  
  if (m_state == 0) {
    raiseErr(__LINE__, NULL);
  }
  if ((i < 0) || (i >= m_acc_len)) {
    raiseErr(__LINE__, NULL);
  }
  if (pr == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if ((pr->lo < 0) || (pr->hi < pr->lo)) {
    raiseErr(__LINE__, NULL);
  }
  
  if (i > 0) {
    if (pr->lo - 2 < (m_acc[i - 1]).hi) {
      raiseErr(__LINE__, NULL);
    }
  }
  
  if (i < m_acc_len - 1) {
    if ((m_acc[i + 1]).lo - 2 < pr->hi) {
      raiseErr(__LINE__, NULL);
    }
  }
  
  memcpy(&(m_acc[i]), pr, sizeof(RANGE));
}

/*
 * Remove a range from the accumulator array.
 * 
 * m_state must not be zero.  (The accumulator must be loaded with
 * something.)
 * 
 * i is the index of the range to delete in the array.  It must be 
 * greater than or equal to zero and less than m_acc_len.
 * 
 * Parameters:
 * 
 *   i - the index of the range object to delete
 */
static void accDelete(int32_t i) {
  
  int32_t j = 0;
  
  if (m_state == 0) {
    raiseErr(__LINE__, NULL);
  }
  if ((i < 0) || (i >= m_acc_len)) {
    raiseErr(__LINE__, NULL);
  }
  
  for(j = i; j < m_acc_len; j++) {
    memcpy(&(m_acc[j]), &(m_acc[j + 1]), sizeof(RANGE));
  }
  
  m_acc_len--;
}

/*
 * Encode a span value and its openess into the encoded integer format
 * used in the SET object table.
 * 
 * Parameters:
 * 
 *   v - the span value to encode, zero or greater
 * 
 *   o - non-zero if this is an open span, zero if closed span
 * 
 * Return:
 * 
 *   the encoded integer value
 */
static int32_t encodeEntry(int32_t v, int o) {
  
  if (v < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  if (o) {
    v = (0 - v) - 1;
  }
  
  return v;
}

/*
 * Decode an encoded integer entry from a SET object table.
 * 
 * e is the entry to decode.
 * 
 * The returned value is the value of the span.  If po is not NULL,
 * non-zero is written to it if this is an open span, or zero if this is
 * a closed span.
 * 
 * Parameters:
 * 
 *   e - the entry to decode
 * 
 *   po - pointer to flag to receive whether this is an open span, or
 *   NULL
 * 
 * Return:
 * 
 *   the value of the span
 */
static int32_t decodeEntry(int32_t e, int *po) {
  
  if (e < 0) {
    if (po != NULL) {
      *po = 1;
    }
    e = 0 - (e + 1);
  
  } else {
    if (po != NULL) {
      *po = 0;
    }
  }
  
  return e;
}

/*
 * Scan through the RANGE objects encoded in a SET object.
 * 
 * Set objects do not actually include RANGE objects, but rather encode
 * their ranges with a scheme described in the documentation for the SET
 * structure.  This function allows the encoded SET to be iterated as if
 * it were a sequence of ranges.
 * 
 * Each returned range specifies values that are included in the set.
 * The ranges are never exclusionary.
 * 
 * Returned ranges may have -1 as their upper boundary to indicate an
 * open range with only a lower boundary.
 * 
 * pi points to an integer index within the encoded set array.  At the
 * start of an iteration, this integer should be set to zero.  The
 * function will update it after each call to be the next index position
 * to process in the next function call.
 * 
 * Once the integer index goes beyond the end of the encoded set data,
 * this function will return a zero value to indicate the iteration is
 * complete.
 * 
 * Parameters:
 * 
 *   ps - the set object to iterate
 * 
 *   pi - pointer to the index, which should be set to zero at the start
 *   of iteration
 * 
 *   pr - the range object to receive the next decoded range
 * 
 * Return:
 * 
 *   non-zero if successful, zero if no further ranges to iterate
 */
static int scanRange(const SET *ps, int32_t *pi, RANGE *pr) {
  
  int status = 1;
  int of = 0;
  int32_t n = 0;
  
  /* Check parameters */
  if ((ps == NULL) || (pi == NULL) || (pr == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  if (*pi < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Check if finished iteration */
  if (*pi >= ps->len) {
    status = 0;
  }
  
  /* Proceed if iteration still in progress */
  if (status) {
    
    /* Decode current entry as the lower bound of the decoded range */
    pr->lo = decodeEntry((ps->table)[*pi], &of);
    
    /* Set upper bound of the decoded range to same as low bound if this
     * is a closed span, or to the special -1 value if this is an open
     * span */
    if (of) {
      pr->hi = -1;
    } else {
      pr->hi = pr->lo;
    }
    
    /* Iterate through the rest of the encoded entries */
    for((*pi)++; *pi < ps->len; (*pi)++) {
      
      /* Decode this entry */
      n = decodeEntry((ps->table)[*pi], &of);
      
      /* If current decoded range is closed and this entry is at least
       * two beyond the upper boundary, we are done so leave loop */
      if ((pr->hi >= 0) && (n - 2 >= pr->hi)) {
        break;
      }
      
      /* If we got here, either the current decoded range is open or the
       * current decoded range is closed but immediately adjacent to the
       * new entry -- if new entry is closed, update upper boundary to
       * this value; if new entry is open, update upper boundary to -1
       * marker value */
      if (of) {
        pr->hi = -1;
      } else {
        pr->hi = n;
      }
    }
  }
  
  /* Return status */
  return status;
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * set_begin function.
 */
void set_begin(long lnum) {
  if (m_shutdown) {
    raiseErr(__LINE__, "Set module is shut down");
  }
  if (m_state != 0) {
    raiseErr(__LINE__,
      "Set definition already in progress on script line %ld",
      srcLine(lnum));
  }
  
  m_state = 1;
  accReset();
}

/*
 * set_all function.
 */
void set_all(long lnum) {
  if (m_shutdown) {
    raiseErr(__LINE__, "Set module is shut down");
  }
  if (m_state == 0) {
    raiseErr(__LINE__,
      "No set definition in progress on script line %ld",
      srcLine(lnum));
  }
  
  m_state = -1;
  accReset();
}

/*
 * set_none function.
 */
void set_none(long lnum) {
  if (m_shutdown) {
    raiseErr(__LINE__, "Set module is shut down");
  }
  if (m_state == 0) {
    raiseErr(__LINE__,
      "No set definition in progress on script line %ld",
      srcLine(lnum));
  }
  
  m_state = 1;
  accReset();
}

/*
 * set_invert function.
 */
void set_invert(long lnum) {
  if (m_shutdown) {
    raiseErr(__LINE__, "Set module is shut down");
  }
  if (m_state == 0) {
    raiseErr(__LINE__,
      "No set definition in progress on script line %ld",
      srcLine(lnum));
  }
  
  if (m_state > 0) {
    m_state = -1;
  } else if (m_state < 0) {
    m_state = 1;
  } else {
    raiseErr(__LINE__, NULL);
  }
}

/*
 * set_rclose function.
 */
void set_rclose(int32_t lo, int32_t hi, int exc, long lnum) {
  
  int32_t i = 0;
  RANGE r;
  RANGE r2;
  
  /* Initialize structures */
  memset(&r, 0, sizeof(RANGE));
  memset(&r2, 0, sizeof(RANGE));
  
  /* Check state and parameters */
  if (m_shutdown) {
    raiseErr(__LINE__, "Set module is shut down");
  }
  if (m_state == 0) {
    raiseErr(__LINE__,
      "No set definition in progress on script line %ld",
      srcLine(lnum));
  }
  
  if ((lo < 0) || (hi < lo)) {
    raiseErr(__LINE__, "Invalid range for set on script line %ld",
              srcLine(lnum));
  }
  
  /* If we have a negative set, then invert the exclusion flag because
   * ranges in a negative set track exclusions rather than inclusions */
  if (m_state < 0) {
    if (exc) {
      exc = 0;
    } else {
      exc = 1;
    }
  }
  
  /* Now perform either inclusion or exclusion */
  if (exc) {
    /* EXCLUSION -- process all ranges in the accumulator */
    for(i = 0; i < m_acc_len; i++) {
      /* If lower bound of current range is beyond the upper bound of
       * excluded range, then this range and all subsequent ranges are
       * unaffected by the exclusion, so leave the loop */
      if (hi < (m_acc[i]).lo) {
        break;
      }
      
      /* If upper bound of current range is beyond the lower bound of
       * excluded range, then skip this range */
      if ((m_acc[i].hi) < lo) {
        continue;
      }
      
      /* We've now handled the cases where both current range boundaries
       * are above the exclusion zone and both current range boundaries
       * are below the exclusion zone; check for the remaining cases */
      if (((m_acc[i]).lo < lo) && ((m_acc[i]).hi > hi)) {
        /* One current range boundary below excluded zone and one
         * current range boundary above excluded zone; begin by making
         * two copies of current range*/
        memcpy(&r, &(m_acc[i]), sizeof(RANGE));
        memcpy(&r2, &(m_acc[i]), sizeof(RANGE));
        
        /* First copy will be from current lower boundary up to just
         * below the excluded zone */
        r.hi = lo - 1;
        
        /* Second copy will be from just above the excluded zone to
         * current upper boundary */
        r2.lo = hi + 1;
        
        /* Replace the current range with the new upper copy and then
         * insert the new lower copy before it; this is equivalent to
         * the original range less the excluded zone */
        accReplace(i, &r2);
        accInsert(i, &r, lnum);
        
        /* Increment i to account for the insertion */
        i++;
      
      } else if (((m_acc[i]).lo >= lo) && ((m_acc[i]).hi > hi)) {
        /* Lower current range boundary in excluded zone and upper
         * current range boundary above excluded zone; begin by making
         * copy of current range */
        memcpy(&r, &(m_acc[i]), sizeof(RANGE));
        
        /* Replace lower current range boundary with just above the
         * excluded zone */
        r.lo = hi + 1;
        
        /* Replace current range so that it is now entirely outside the
         * excluded zone */
        accReplace(i, &r);
        
      } else if (((m_acc[i]).lo < lo) && ((m_acc[i]).hi <= hi)) {
        /* Upper current range boundary in excluded zone and lower
         * current range boundary below excluded zone; begin by making
         * copy of current range */
        memcpy(&r, &(m_acc[i]), sizeof(RANGE));
        
        /* Replace upper current range boundary with just below the
         * excluded zone */
        r.hi = lo - 1;
        
        /* Replace current range so that it is now entirely outside the
         * excluded zone */
        accReplace(i, &r);
      
      } else if (((m_acc[i]).lo >= lo) && ((m_acc[i]).hi <= hi)) {
        /* Both current range boundaries in excluded zone; delete
         * current range and decrement i to account for the deletion */
        accDelete(i);
        i--;
      
      } else {
        raiseErr(__LINE__, NULL);
      }
    }
    
  } else {
    /* INCLUSION -- process all ranges in the accumulator */
    for(i = 0; i < m_acc_len; i++) {
      /* If lower bound of current range is two beyond the upper bound
       * of new range, then this range and all subsequent ranges have
       * both boundaries above and not touching the new range, so leave
       * the loop with i now indicating where we should insert the new
       * range before */
      if (hi <= (m_acc[i]).lo - 2) {
        break;
      }
      
      /* If upper bound of current range is two beyond the lower bound
       * of new range, then this range is not touching the new range, so
       * skip it with no updates */
      if ((m_acc[i]).hi <= lo - 2) {
        continue;
      }
      
      /* If we got here, then current range is touching the new range
       * somehow, so first expand the new range if necessary so it
       * includes the current range */
      if ((m_acc[i]).lo < lo) {
        lo = (m_acc[i]).lo;
      }
      if ((m_acc[i]).hi > hi) {
        hi = (m_acc[i]).hi;
      }
      
      /* Delete current range because new range will include it, then
       * decrement i to account for the deletion */
      accDelete(i);
      i--;
    }

    /* If we left the loop without finding a range entirely above the
     * new range, set i to -1 so we will append the new range to the end
     * of the accumulator array; else, leave i as the first range
     * entirely above the new range */
    if (i >= m_acc_len) {
      i = -1;
    }
    
    /* Insert or append the new range */
    r.lo = lo;
    r.hi = hi;

    accInsert(i, &r, lnum);
  }
}

/*
 * set_ropen function.
 */
void set_ropen(int32_t lo, int exc, long lnum) {
  
  int inv = 0;
  int32_t i = 0;
  int32_t pos = -1;
  RANGE r;
  
  /* Initialize structures */
  memset(&r, 0, sizeof(RANGE));
  
  /* Check state and parameters */
  if (m_shutdown) {
    raiseErr(__LINE__, "Set module is shut down");
  }
  if (m_state == 0) {
    raiseErr(__LINE__,
      "No set definition in progress on script line %ld",
      srcLine(lnum));
  }
  if (lo < 0) {
    raiseErr(__LINE__, "Invalid range for set on script line %ld",
              srcLine(lnum));
  }
  
  /* If we are including an open range in a positive set or excluding an
   * open range from a negative set, we will need to reverse the
   * polarity of the set; otherwise, the polarity stays */
  if (((!exc) && (m_state > 0)) || (exc && (m_state < 0))) {
    inv = 1;
  } else {
    inv = 0;
  }
  
  /* Perform appropriate operation */
  if (inv) {
    /* INVERT POLARITY -- begin by expanding open range to include any
     * existing ranges it touches and deleting those ranges */
    for(i = 0; i < m_acc_len; i++) {
      if ((m_acc[i]).hi > lo - 2) {
        lo = (m_acc[i]).lo;
        accDelete(i);
        i--;
      }
    }
    
    /* Position starts at -1, meaning we don't have any range carried
     * over */
    pos = -1;
    
    /* Iterate through ranges again and this time replace each range
     * with the medial zone before it to effect the polarity
     * inversion */
    for(i = 0; i < m_acc_len; i++) {
      if ((pos < 0) && ((m_acc[i]).lo > 0)) {
        /* No range is carried over (meaning this is the first range)
         * and the current range starts above zero, so replace current
         * range with range from zero to just below current range, and
         * update pos to just after current range */
        memcpy(&r, &(m_acc[i]), sizeof(RANGE));
        
        pos  = r.hi + 1;
        r.hi = r.lo - 1;
        r.lo = 0;
        
        accReplace(i, &r);
        
      } else if ((pos < 0) && ((m_acc[i]).lo < 1)) {
        /* No range is carried over (meaning this is the first range)
         * and the current range starts at zero, so delete current range
         * and update pos to just after current range */
        pos = (m_acc[i]).hi + 1;
        accDelete(i);
        i--;
        
      } else if (pos >= 0) {
        /* Range carried over, so replace current range with position up
         * to just before the current range, and then update position to
         * just after current range */
        r.lo = pos;
        r.hi = (m_acc[i]).lo - 1;
        pos  = (m_acc[i]).hi + 1;
        
        accReplace(i, &r);
        
      } else {
        raiseErr(__LINE__, NULL);
      }
    }
    
    /* If present, append the last medial zone just before the open
     * range */
    if ((pos < 0) && (lo > 0)) {
      /* There weren't any ranges but the start of the open range is
       * above zero, so append range from zero to just before the open
       * range */
      r.lo = 0;
      r.hi = lo - 1;
      
      accInsert(-1, &r, lnum);
      
    } else if (pos >= 0) {
      /* There was at least one range, so append range from the carried
       * over position to just before the open range */
      r.lo = pos;
      r.hi = lo - 1;
      
      accInsert(-1, &r, lnum);
    }
    
    /* Invert polarity of set */
    if (m_state > 0) {
      m_state = -1;
    
    } else if (m_state < 0) {
      m_state = 1;
    
    } else {
      raiseErr(__LINE__, NULL);
    }
    
  } else {
    /* KEEP POLARITY -- process all ranges */
    for(i = 0; i < m_acc_len; i++) {
      if ((m_acc[i]).lo >= lo) {
        /* Current range entirely within open range, so drop it */
        accDelete(i);
        i--;
        
      } else if ((m_acc[i]).hi >= lo) {
        /* Current range upper boundary within open range but lower
         * boundary not, so replace upper boundary with just before the
         * open range */
        memcpy(&r, &(m_acc[i]), sizeof(RANGE));
        r.hi = lo - 1;
        accReplace(i, &r);
      }
    }
  }
}

/*
 * set_union function.
 */
void set_union(SET *ps, long lnum) {
  
  int32_t i = 0;
  int retval = 0;
  RANGE r;
  
  memset(&r, 0, sizeof(RANGE));
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Set module is shut down");
  }
  if (m_state == 0) {
    raiseErr(__LINE__,
      "No set definition in progress on script line %ld",
      srcLine(lnum));
  }
  if (ps == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  i = 0;
  for(retval = scanRange(ps, &i, &r);
      retval;
      retval = scanRange(ps, &i, &r)) {
    
    if (r.hi >= 0) {
      set_rclose(r.lo, r.hi, 0, lnum);
    } else {
      set_ropen(r.lo, 0, lnum);
    }
  }
}

/*
 * set_intersect function.
 */
void set_intersect(SET *ps, long lnum) {
  
  int32_t pos = 0;
  int32_t i = 0;
  int retval = 0;
  RANGE r;
  
  memset(&r, 0, sizeof(RANGE));
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Set module is shut down");
  }
  if (m_state == 0) {
    raiseErr(__LINE__,
      "No set definition in progress on script line %ld",
      srcLine(lnum));
  }
  if (ps == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  i = 0;
  pos = 0;
  for(retval = scanRange(ps, &i, &r);
      retval;
      retval = scanRange(ps, &i, &r)) {
    
    if (pos < 0) {
      raiseErr(__LINE__, NULL);
    }
    
    if (r.lo > pos) {
      set_rclose(pos, r.lo - 1, 1, lnum);
    }
    
    if ((r.hi >= 0) && (r.hi < INT32_MAX)) {
      pos = r.hi + 1;
    } else {
      pos = -1;
    }
  }
  
  if (pos >= 0) {
    set_ropen(pos, 1, lnum);
  }
}

/*
 * set_except function.
 */
void set_except(SET *ps, long lnum) {
  
  int32_t i = 0;
  int retval = 0;
  RANGE r;
  
  memset(&r, 0, sizeof(RANGE));
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Set module is shut down");
  }
  if (m_state == 0) {
    raiseErr(__LINE__,
      "No set definition in progress on script line %ld",
      srcLine(lnum));
  }
  if (ps == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  i = 0;
  for(retval = scanRange(ps, &i, &r);
      retval;
      retval = scanRange(ps, &i, &r)) {
    
    if (r.hi >= 0) {
      set_rclose(r.lo, r.hi, 1, lnum);
    } else {
      set_ropen(r.lo, 1, lnum);
    }
  }
}

/*
 * set_end function.
 */
SET *set_end(long lnum) {
  
  SET *ps = NULL;
  int32_t count = 0;
  int32_t pos = 0;
  int32_t i = 0;
  int32_t *pt = NULL;
  size_t sz = 0;
  
  /* Check state */
  if (m_shutdown) {
    raiseErr(__LINE__, "Set module is shut down");
  }
  if (m_state == 0) {
    raiseErr(__LINE__,
      "No set definition in progress on script line %ld",
      srcLine(lnum));
  }
  
  /* Operation differs depending on polarity */
  if (m_state > 0) {
    /* Positive polarity, so each single-value range will require a
     * single encoded entry and each multi-value range will require two
     * encoded entries */
    count = 0;
    for(i = 0; i < m_acc_len; i++) {
      if ((m_acc[i]).lo == (m_acc[i]).hi) {
        count++;
      } else {
        count += 2;
      }
      
      if (count > SET_MAX_TABLE) {
        raiseErr(__LINE__, "Set too complex on script line %ld",
                  srcLine(lnum));
      }
    }

    /* Allocate the set structure */
    if (count > 0) {
      sz = (((size_t) (count - 1)) * sizeof(int32_t)) + sizeof(SET);
    } else {
      sz = sizeof(SET);
    }
    
    ps = (SET *) calloc(1, sz);
    if (ps == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }
    
    /* Generate the set table */
    pt = &(ps->table[0]);
    ps->len = count;

    for(i = 0; i < m_acc_len; i++) {
      if ((m_acc[i]).lo == (m_acc[i]).hi) {
        /* Single-element range, so encode as closed span */
        *pt = encodeEntry((m_acc[i]).lo, 0);
        pt++;
        
      } else if ((m_acc[i]).lo == (m_acc[i]).hi - 1) {
        /* Two-element range, so encode as two closed spans */
        *pt = encodeEntry((m_acc[i]).lo, 0);
        pt++;
        
        *pt = encodeEntry((m_acc[i]).hi, 0);
        pt++;
        
      } else {
        /* Three or more element range, so encode as an open span
         * followed by a closed span */
        *pt = encodeEntry((m_acc[i]).lo, 1);
        pt++;
        
        *pt = encodeEntry((m_acc[i]).hi, 0);
        pt++;
      }
    }
    
  } else if (m_state < 0) {
    /* Negative polarity, so each range that has a gap of one element
     * before it requires a single entry, each range that has a gap of
     * multiple elements before it requires two entries, and one
     * additional range required for the remaining elements unless the
     * last range reaches the maximum integer value */
    count = 0;
    pos = 0;
    
    for(i = 0; i < m_acc_len; i++) {
      if (pos < 0) {
        raiseErr(__LINE__, NULL);
      }
      
      if ((m_acc[i]).lo - 1 == pos) {
        count++;
      } else if ((m_acc[i]).lo - 2 >= pos) {
        count += 2;
      }
      
      if (count > SET_MAX_TABLE) {
        raiseErr(__LINE__, "Set too complex on script line %ld",
                  srcLine(lnum));
      }
      
      if ((m_acc[i]).hi < INT32_MAX) {
        pos = (m_acc[i]).hi + 1;
      } else {
        pos = -1;
      }
    }
    
    if (pos >= 0) {
      count++;
    }
    if (count > SET_MAX_TABLE) {
      raiseErr(__LINE__, "Set too complex on script line %ld",
                srcLine(lnum));
    }
    
    /* Allocate the set structure */
    if (count > 0) {
      sz = (((size_t) (count - 1)) * sizeof(int32_t)) + sizeof(SET);
    } else {
      sz = sizeof(SET);
    }
    
    ps = (SET *) calloc(1, sz);
    if (ps == NULL) {
      raiseErr(__LINE__, "Out of memory");
    }
    
    /* Generate the set table */
    pt = &(ps->table[0]);
    ps->len = count;
    
    pos = 0;
    for(i = 0; i < m_acc_len; i++) {
      if (pos < 0) {
        raiseErr(__LINE__, NULL);
      }
      
      if ((m_acc[i]).lo - 1 == pos) {
        /* Exactly one element in gap before range, so encode the gap
         * with a closed span */
        *pt = encodeEntry(pos, 0);
        pt++;
        
      } else if ((m_acc[i]).lo - 2 == pos) {
        /* Exactly two elements in gap before range, so encode the gap
         * with two closed spans */
        *pt = encodeEntry(pos, 0);
        pt++;
        
        *pt = encodeEntry(pos + 1, 0);
        pt++;
        
      } else if ((m_acc[i]).lo - 3 >= pos) {
        /* Three or more elements in gap before range, so encode the gap
         * with an open span followed by a closed span */
        *pt = encodeEntry(pos, 1);
        pt++;
        
        *pt = encodeEntry((m_acc[i]).lo - 1, 0);
        pt++;
      }
      
      if ((m_acc[i]).hi < INT32_MAX) {
        pos = (m_acc[i]).hi + 1;
      } else {
        pos = -1;
      }
    }
    
    if (pos >= 0) {
      *pt = encodeEntry(pos, 1);
      pt++;
    }
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  /* Add set to set chain */
  if (m_pLast == NULL) {
    m_pFirst = ps;
    m_pLast = ps;
    ps->pNext = NULL;
  } else {
    m_pLast->pNext = ps;
    ps->pNext = NULL;
    m_pLast = ps;
  }
  
  /* Reset accumulator state */
  m_state = 0;
  accReset();
  
  /* Return the new set */
  return ps;
}

/*
 * set_shutdown function.
 */
void set_shutdown(void) {
  SET *pCur = NULL;
  SET *pNext = NULL;
  
  if (!m_shutdown) {
    m_state = 0;
    accReset();
    
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
 * set_has function.
 */
int set_has(SET *ps, int32_t val) {
  
  int result = 0;
  int32_t lo = 0;
  int32_t hi = 0;
  int32_t mid = 0;
  int32_t v = 0;
  int o = 0;
  
  /* Check state and parameters */
  if (m_shutdown) {
    raiseErr(__LINE__, "Set module is shut down");
  }
  if (ps == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if (val < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Only proceed if non-empty set table */
  if (ps->len > 0) {
    /* Start with entire table as search range */
    lo = 0;
    hi = ps->len - 1;
    
    /* Zoom in the range */
    while (lo < hi) {
      
      /* Choose midpoint as halfway between, but also make sure at least
       * one above the lower boundary */
      mid = lo + ((hi - lo) / 2);
      if (mid <= lo) {
        mid = lo + 1;
      }
      
      /* Compare midpoint span value to search value */
      v = decodeEntry((ps->table)[mid], NULL);
      if (v < val) {
        /* Midpoint value less than search value, so midpoint is new
         * lower boundary of the search */
        lo = mid;
        
      } else if (v > val) {
        /* Midpoint value greater than search value, so one below
         * midpoint is new upper boundary of the search */
        hi = mid - 1;
        
      } else if (v == val) {
        /* We found the value, so zoom in on it */
        lo = mid;
        hi = mid;
        
      } else {
        raiseErr(__LINE__, NULL);
      }
    }
    
    /* Range is zoomed in, so get the span and check whether the search
     * value is in it */
    v = decodeEntry((ps->table)[lo], &o);
    
    if (o) {
      if (val >= v) {
        result = 1;
      }
      
    } else {
      if (val == v) {
        result = 1;
      }
    }
  }
  
  /* Return result */
  return result;
}

/*
 * set_print function.
 */
void set_print(SET *ps, FILE *pOut) {
  
  int32_t i = 0;
  int retval = 0;
  int is_filled = 0;
  RANGE r;
  
  memset(&r, 0, sizeof(RANGE));
  
  if (m_shutdown) {
    raiseErr(__LINE__, "Set module is shut down");
  }
  if ((ps == NULL) || (pOut == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  for(retval = scanRange(ps, &i, &r);
      retval;
      retval = scanRange(ps, &i, &r)) {
    
    if (is_filled) {
      fprintf(pOut, ",");
    }
    is_filled = 1;
    
    if (r.hi < 0) {
      fprintf(pOut, "%ld-", (long) r.lo);
      
    } else if (r.lo == r.hi) {
      fprintf(pOut, "%ld", (long) r.hi);
      
    } else {
      fprintf(pOut, "%ld-%ld", (long) r.lo, (long) r.hi);
    }
  }
  
  if (!is_filled) {
    fprintf(pOut, "<empty>");
  }
}
