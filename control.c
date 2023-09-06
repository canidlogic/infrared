/*
 * control.c
 * =========
 * 
 * Implementation of control.h
 * 
 * See header for further information.
 */

#include "control.h"

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostic.h"
#include "midi.h"

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
 * The initial and maximum capacity of the dynamic array holding the
 * controller mapping records.
 */
#define MAP_INIT_CAP (32)
#define MAP_MAX_CAP  (16384)

/*
 * Type declarations
 * =================
 */

/*
 * Structure storing a mapping between a specific controller and a
 * graph.
 */
typedef struct {
  
  /*
   * One of the CONTROL_TYPE constants indicating the controller type.
   */
  int8_t ctype;
  
  /*
   * The MIDI channel of the controller.
   * 
   * Ignored and set to zero for TEMPO controller.
   */
  int8_t ch;
  
  /*
   * The index of the controller.
   * 
   * Only used for 7BIT, 14BIT, NONREG, and REG controllers.
   */
  int16_t idx;
  
  /*
   * The graph mapped to this controller.
   */
  GRAPH *pg;

} CTL_MAP;

/*
 * Local data
 * ==========
 */

/*
 * The controller mapping array.
 * 
 * The fields here are the current capacity as a record count, the
 * number of records actually in use, and a pointer to the dynamically
 * allocated record array.
 * 
 * The array elements must be sorted first by control type, then by
 * channel, and then by index.
 */
static int32_t m_map_cap = 0;
static int32_t m_map_len = 0;
static CTL_MAP *m_map = NULL;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static long srcLine(long lnum);
static void capMap(int32_t n);
static int32_t seekMap(int ctype, int ch, int idx);
static void trackCtl(void *pCustom, int32_t t, int32_t v);

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
 * Make room in capacity for a given number of elements in the map
 * array.
 * 
 * n is the number of additional elements beyond current length to make
 * room for.  It must be zero or greater.
 * 
 * Upon return, the capacity will be at least n elements higher than the
 * current length.
 * 
 * An error occurs if the requested expansion would go beyond the
 * maximum allowed capacity.
 * 
 * Parameters:
 * 
 *   n - the number of elements to make room for
 */
static void capMap(int32_t n) {
  
  int32_t target = 0;
  int32_t new_cap = 0;
  
  /* Check parameters */
  if (n < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Only proceed if n is non-zero */
  if (n > 0) {
    
    /* Make initial allocation if necessary */
    if (m_map_cap < 1) {
      m_map = (CTL_MAP *) calloc(
                  (size_t) MAP_INIT_CAP, sizeof(CTL_MAP));
      if (m_map == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      m_map_cap = MAP_INIT_CAP;
      m_map_len = 0;
    }
    
    /* Compute target length */
    if (n <= INT32_MAX - m_map_len) {
      target = m_map_len + n;
    } else {
      raiseErr(__LINE__, "Control map capacity exceeded");
    }
    
    /* Only proceed if target length exceeds current capacity */
    if (target > m_map_cap) {
      /* Check that target within maximum capacity */
      if (target > MAP_MAX_CAP) {
        raiseErr(__LINE__, "Control map capacity exceeded");
      }
      
      /* Compute new capacity by doubling current capacity until greater
       * than or equal to target length, and then limiting to maximum
       * capacity */
      new_cap = m_map_cap;
      while (new_cap < target) {
        new_cap *= 2;
      }
      if (new_cap > MAP_MAX_CAP) {
        new_cap = MAP_MAX_CAP;
      }
      
      /* Expand capacity */
      m_map = (CTL_MAP *) realloc(m_map,
                            ((size_t) new_cap) * sizeof(CTL_MAP));
      if (m_map == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      memset(
        &(m_map[m_map_cap]),
        0,
        ((size_t) (new_cap - m_map_cap)) * sizeof(CTL_MAP));
      
      m_map_cap = new_cap;
    }
  }
}

/*
 * Find the index in the sorted map array of the least element that is
 * greater than or equal to the given key value, or -1 if there is no
 * such element.
 * 
 * This function does not check that the given key values are in range.
 * 
 * Parameters:
 * 
 *   ctype - the control type to seek for
 * 
 *   ch - the MIDI channel to seek for
 * 
 *   idx - the controller index to seek for
 * 
 * Return:
 * 
 *   the index of the least element greater than or equal to the given
 *   key value, or -1 if no such element
 */
static int32_t seekMap(int ctype, int ch, int idx) {
  
  int32_t result = -1;
  int32_t lo = 0;
  int32_t hi = 0;
  int32_t mid = 0;
  int c = 0;
  
  /* Only proceed if array is not empty */
  if (m_map_len > 0) {
    
    /* Zoom in on a single element */
    lo = 0;
    hi = m_map_len - 1;
    while (lo < hi) {
      
      /* Choose a midpoint halfway between and also at least one below
       * the upper bound */
      mid = lo + ((hi - lo) / 2);
      if (mid >= hi) {
        mid = hi - 1;
      }
      
      /* Compare current value to given key value */
      c = 0;
      
      if (ctype < (m_map[mid]).ctype) {
        c = -1;
      } else if (ctype > (m_map[mid]).ctype) {
        c = 1;
      }
      
      if (c == 0) {
        if (ch < (m_map[mid]).ch) {
          c = -1;
        } else if (ch > (m_map[mid]).ch) {
          c = 1;
        }
      }
      
      if (c == 0) {
        if (idx < (m_map[mid]).idx) {
          c = -1;
        } else if (idx > (m_map[mid]).idx) {
          c = 1;
        }
      }
      
      /* Update range depending on comparison */
      if (c > 0) {
        /* Search key is greater than midpoint, so new search range
         * begins above the midpoint */
        lo = mid + 1;
        
      } else if (c < 0) {
        /* Search key is less than midpoint, so new search range ends at
         * the midpoint */
        hi = mid;
        
      } else if (c == 0) {
        /* Search key equals midpoint, so zoom in */
        lo = mid;
        hi = mid;
        
      } else {
        raiseErr(__LINE__, NULL);
      }
    }
    
    /* Compare the chosen element to given key value */
    c = 0;
    
    if (ctype < (m_map[lo]).ctype) {
      c = -1;
    } else if (ctype > (m_map[lo]).ctype) {
      c = 1;
    }
    
    if (c == 0) {
      if (ch < (m_map[lo]).ch) {
        c = -1;
      } else if (ch > (m_map[lo]).ch) {
        c = 1;
      }
    }
    
    if (c == 0) {
      if (idx < (m_map[lo]).idx) {
        c = -1;
      } else if (idx > (m_map[lo]).idx) {
        c = 1;
      }
    }
    
    /* If search key is less than or equal to chosen element, chosen
     * element is the result; else, -1 is the result */
    if (c <= 0) {
      result = lo;
    } else {
      result = -1;
    }
  }
  
  /* Return result */
  return result;
}

/*
 * Graph tracking callback for generating automatic control messages.
 * 
 * The interface of this function matches the graph_fp_track function
 * pointer type.
 * 
 * The custom pointer must be to the CTL_MAP structure that is currently
 * being tracked.
 * 
 * Parameters:
 * 
 *   pCustom - the CTL_MAP structure passed through
 * 
 *   t - the moment offset of graph change in value
 * 
 *   v - the new graph value
 */
static void trackCtl(void *pCustom, int32_t t, int32_t v) {
  
  const CTL_MAP *pe = NULL;
  int a = 0;
  int b = 0;
  int ia = 0;
  int ib = 0;
  
  /* Check parameters and get map element pointer */
  if (pCustom == NULL) {
    raiseErr(__LINE__, NULL);
  }
  pe = (const CTL_MAP *) pCustom;
  
  /* Different handling depending on controller type */
  if (pe->ctype == CONTROL_TYPE_TEMPO) { /* ========================= */
    /* Check graph value */
    if ((v < MIDI_TEMPO_MIN) || (v > MIDI_TEMPO_MAX)) {
      raiseErr(__LINE__, "Tempo graph value out of range");
    }
    
    /* Generate automatic message */
    midi_tempo(t, 0, (int) v);
    
  } else if (pe->ctype == CONTROL_TYPE_7BIT) { /* =================== */
    /* Check graph value */
    if ((v < 0) || (v > MIDI_DATA_MAX)) {
      raiseErr(__LINE__, "7-bit controller graph value out of range");
    }
    
    /* Generate automatic message */
    midi_message(
      t, 0, (int) pe->ch, MIDI_MSG_CONTROL, (int) pe->idx, (int) v);
    
  } else if (pe->ctype == CONTROL_TYPE_14BIT) { /* ================== */
    /* Check graph value */
    if ((v < 0) || (v > CONTROL_MAX_14BIT)) {
      raiseErr(__LINE__, "14-bit controller graph value out of range");
    }
    
    /* Parse value into most significant (a) and least significant (b)
     * 7-bit data bytes */
    a = (int) ((v >> 7) & 0x7f);
    b = (int) ( v       & 0x7f);
    
    /* Generate automatic message */
    midi_message(
      t, 0, (int) pe->ch, MIDI_MSG_CONTROL, (int) pe->idx, a);
    midi_message(
      t, 0, (int) pe->ch, MIDI_MSG_CONTROL, (int) (pe->idx + 0x20), b);
    
  } else if (pe->ctype == CONTROL_TYPE_NONREG) { /* ================= */
    /* Check graph value */
    if ((v < 0) || (v > CONTROL_MAX_14BIT)) {
      raiseErr(__LINE__,
        "Non-registered controller graph value out of range");
    }
    
    /* Parse value into most significant (a) and least significant (b)
     * 7-bit data bytes */
    a = (int) ((v >> 7) & 0x7f);
    b = (int) ( v       & 0x7f);
    
    /* Parse index into most significant (ia) and least significant (ib)
     * 7-bit data types */
    ia = (int) ((pe->idx >> 7) & 0x7f);
    ib = (int) ( pe->idx       & 0x7f);
    
    /* Generate automatic message */
    midi_message(
      t, 0, (int) pe->ch, MIDI_MSG_CONTROL, 0x62, ib);
    midi_message(
      t, 0, (int) pe->ch, MIDI_MSG_CONTROL, 0x63, ia);
    midi_message(
      t, 0, (int) pe->ch, MIDI_MSG_CONTROL, 0x06, a);
    midi_message(
      t, 0, (int) pe->ch, MIDI_MSG_CONTROL, 0x26, b);
    
  } else if (pe->ctype == CONTROL_TYPE_REG) { /* ==================== */
    /* Check graph value */
    if ((v < 0) || (v > CONTROL_MAX_14BIT)) {
      raiseErr(__LINE__,
        "Registered controller graph value out of range");
    }
    
    /* Parse value into most significant (a) and least significant (b)
     * 7-bit data bytes */
    a = (int) ((v >> 7) & 0x7f);
    b = (int) ( v       & 0x7f);
    
    /* Parse index into most significant (ia) and least significant (ib)
     * 7-bit data types */
    ia = (int) ((pe->idx >> 7) & 0x7f);
    ib = (int) ( pe->idx       & 0x7f);
    
    /* Generate automatic message */
    midi_message(
      t, 0, (int) pe->ch, MIDI_MSG_CONTROL, 0x64, ib);
    midi_message(
      t, 0, (int) pe->ch, MIDI_MSG_CONTROL, 0x65, ia);
    midi_message(
      t, 0, (int) pe->ch, MIDI_MSG_CONTROL, 0x06, a);
    midi_message(
      t, 0, (int) pe->ch, MIDI_MSG_CONTROL, 0x26, b);
    
  } else if (pe->ctype == CONTROL_TYPE_PRESSURE) { /* =============== */
    /* Check graph value */
    if ((v < 0) || (v > MIDI_DATA_MAX)) {
      raiseErr(__LINE__, "Channel pressure graph value out of range");
    }
    
    /* Generate automatic message */
    midi_message(
      t, 0, (int) pe->ch, MIDI_MSG_CH_AFTERTOUCH, 0, (int) v);
    
  } else if (pe->ctype == CONTROL_TYPE_PITCH) { /* ================== */
    /* Check graph value */
    if ((v < 0) || (v > CONTROL_MAX_14BIT)) {
      raiseErr(__LINE__, "Pitch bend graph value out of range");
    }
    
    /* Generate automatic message */
    midi_message(
      t, 0, (int) pe->ch, MIDI_MSG_PITCH_BEND, 0, (int) v);
    
  } else { /* ======================================================= */
    raiseErr(__LINE__, NULL);
  }
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * control_null function.
 */
void control_null(POINTER *pp, long lnum) {
  
  int32_t t = 0;
  int head = 0;
  
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if (pointer_isHeader(pp)) {
    head = 1;
  } else {
    head = 0;
    t = pointer_compute(pp, lnum);
  }
  
  midi_null(t, head);
}

/*
 * control_text function.
 */
void control_text(POINTER *pp, int tclass, TEXT *pText, long lnum) {
  
  int32_t t = 0;
  int head = 0;
  
  if ((pp == NULL) || (pText == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  if ((tclass < MIDI_TEXT_MIN_VAL) || (tclass > MIDI_TEXT_MAX_VAL)) {
    raiseErr(__LINE__, NULL);
  }
  
  if (pointer_isHeader(pp)) {
    head = 1;
  } else {
    head = 0;
    t = pointer_compute(pp, lnum);
  }
  
  midi_text(t, head, tclass, pText);
}

/*
 * control_time_sig function.
 */
void control_time_sig(
    POINTER * pp,
    int32_t   num,
    int32_t   denom,
    int32_t   metro,
    long      lnum) {
  
  int32_t t = 0;
  int head = 0;
  int32_t v = 0;
  
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if ((num < 1) || (num > MIDI_TIME_NUM_MAX)) {
    raiseErr(__LINE__,
      "Time signature numerator out of range on script line %ld",
      srcLine(lnum));
  }
  if ((denom < 1) || (denom > MIDI_TIME_DENOM_MAX)) {
    raiseErr(__LINE__,
      "Time signature denominator out of range on script line %ld",
      srcLine(lnum));
  }
  if ((metro < 1) || (metro > MIDI_TIME_METRO_MAX)) {
    raiseErr(__LINE__,
      "Time signature metronome out of range on script line %ld",
      srcLine(lnum));
  }
  
  v = denom;
  while (v > 1) {
    if ((v % 2) != 0) {
      v /= 2;
    } else {
      raiseErr(__LINE__,
        "Time denominator must be power of 2 on script line %ld",
        srcLine(lnum));
    }
  }
  
  if (pointer_isHeader(pp)) {
    head = 1;
  } else {
    head = 0;
    t = pointer_compute(pp, lnum);
  }
  
  midi_time_sig(t, head, num, denom, metro);
}

/*
 * control_key_sig function.
 */
void control_key_sig(
    POINTER * pp,
    int32_t   count,
    int32_t   minor,
    long      lnum) {
  
  int32_t t = 0;
  int head = 0;
  
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if ((count < MIDI_KEY_COUNT_MIN) || (count > MIDI_KEY_COUNT_MAX)) {
    raiseErr(__LINE__, 
      "Key signature count out of range on script line %ld",
      srcLine(lnum));
  }
  if ((minor != 0) && (minor != 1)) {
    raiseErr(__LINE__,
      "Key signature mode out of range on script line %ld",
      srcLine(lnum));
  }
  
  if (pointer_isHeader(pp)) {
    head = 1;
  } else {
    head = 0;
    t = pointer_compute(pp, lnum);
  }
  
  midi_key_sig(t, head, (int) count, (int) minor);
}

/*
 * control_custom function.
 */
void control_custom(POINTER *pp, BLOB *pData, long lnum) {
  
  int32_t t = 0;
  int head = 0;
  
  if ((pp == NULL) || (pData == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  if (pointer_isHeader(pp)) {
    head = 1;
  } else {
    head = 0;
    t = pointer_compute(pp, lnum);
  }
  
  midi_custom(t, head, pData);
}

/*
 * control_system function.
 */
void control_system(POINTER *pp, BLOB *pData, long lnum) {
  
  int32_t t = 0;
  int head = 0;
  
  if ((pp == NULL) || (pData == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  if (pointer_isHeader(pp)) {
    head = 1;
  } else {
    head = 0;
    t = pointer_compute(pp, lnum);
  }
  
  midi_system(t, head, pData);
}

/*
 * control_instrument function.
 */
void control_instrument(
    POINTER * pp,
    int32_t   ch,
    int32_t   bank,
    int32_t   program,
    int       has_bank,
    long      lnum) {
  
  int32_t t = 0;
  int head = 0;
  int a = 0;
  int b = 0;
  
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if ((ch < 1) || (ch > MIDI_CH_MAX)) {
    raiseErr(__LINE__, "MIDI channel out of range on script line %ld",
              srcLine(lnum));
  }
  if (has_bank) {
    if ((bank < 1) || (bank > 16384)) {
      raiseErr(__LINE__, "MIDI bank out of range on script line %ld",
                srcLine(lnum));
    }
  }
  if ((program < 1) || (program > 128)) {
    raiseErr(__LINE__, "MIDI program out of range on script line %ld",
                srcLine(lnum));
  }
  
  if (pointer_isHeader(pp)) {
    head = 1;
  } else {
    head = 0;
    t = pointer_compute(pp, lnum);
  }
  
  if (has_bank) {
    a = (int) (((bank - 1) >> 7) & 0x7f);
    b = (int) ( (bank - 1)       & 0x7f);
    
    midi_message(t, head, (int) ch, MIDI_MSG_CONTROL,    0, a);
    midi_message(t, head, (int) ch, MIDI_MSG_CONTROL, 0x20, b);
  }
  
  midi_message(
    t, head, (int) ch, MIDI_MSG_PROGRAM, 0, (int) (program - 1));
}

/*
 * control_modal function.
 */
void control_modal(
    POINTER * pp,
    int32_t   ch,
    int       mtype,
    int32_t   count,
    long      lnum) {
  
  int32_t t = 0;
  int head = 0;
  
  if (pp == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if ((ch < 1) || (ch > MIDI_CH_MAX)) {
    raiseErr(__LINE__, "MIDI channel out of range on script line %ld",
              srcLine(lnum));
  }
  if (((mtype < CONTROL_MODE_MIN) || (mtype > CONTROL_MODE_MAX)) &&
        (mtype != CONTROL_MODE_LOCAL_ON)) {
    raiseErr(__LINE__, NULL);
  }
  if (mtype == CONTROL_MODE_MONO) {
    if ((count < 0) || (count > MIDI_CH_MAX)) {
      raiseErr(__LINE__,
        "MIDI mono channel count out of range on script line %ld",
        srcLine(lnum));
    }
  }
  
  if (pointer_isHeader(pp)) {
    head = 1;
  } else {
    head = 0;
    t = pointer_compute(pp, lnum);
  }
  
  if (mtype == CONTROL_MODE_LOCAL_ON) {
    midi_message(t, head, (int) ch, MIDI_MSG_CONTROL, 122, 127);
    
  } else if (mtype == CONTROL_MODE_MONO) {
    midi_message(
      t, head, (int) ch, MIDI_MSG_CONTROL, mtype, (int) count);
      
  } else {
    midi_message(t, head, (int) ch, MIDI_MSG_CONTROL, mtype, 0);
  }
}

/*
 * control_auto function.
 */
void control_auto(
    int       ctype,
    int32_t   ch,
    int32_t   idx,
    GRAPH   * pg,
    long      lnum) {
  
  int32_t i = 0;
  int32_t j = 0;
  int eq = 0;
  
  /* Check parameters */
  if ((ctype < CONTROL_TYPE_MIN) || (ctype > CONTROL_TYPE_MAX)) {
    raiseErr(__LINE__, NULL);
  }
  
  if (ctype != CONTROL_TYPE_TEMPO) {
    if ((ch < 1) || (ch > MIDI_CH_MAX)) {
      raiseErr(__LINE__, "MIDI channel out of range on script line %ld",
                srcLine(lnum));
    }
  }
  
  if (ctype == CONTROL_TYPE_7BIT) {
    if (((idx < CONTROL_INDEX_7BIT_1_MIN) ||
          (idx > CONTROL_INDEX_7BIT_1_MAX)) &&
        ((idx < CONTROL_INDEX_7BIT_2_MIN) ||
          (idx > CONTROL_INDEX_7BIT_2_MAX))) {
      raiseErr(__LINE__,
        "MIDI controller index out of range on script line %ld",
        srcLine(lnum));
    }
    
  } else if (ctype == CONTROL_TYPE_14BIT) {
    if ((idx < CONTROL_INDEX_14BIT_MIN) ||
          (idx > CONTROL_INDEX_14BIT_MAX) ||
          (idx == CONTROL_INDEX_DATA)) {
      raiseErr(__LINE__,
        "MIDI controller index out of range on script line %ld",
        srcLine(lnum));
    }
    
  } else if ((ctype == CONTROL_TYPE_NONREG) ||
              (ctype == CONTROL_TYPE_REG)) {
    if ((idx < 0) || (idx > CONTROL_MAX_14BIT)) {
      raiseErr(__LINE__,
        "MIDI controller index out of range on script line %ld",
        srcLine(lnum));
    }
  }
  
  if (pg == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Set unused key fields to zero */
  if (ctype == CONTROL_TYPE_TEMPO) {
    ch = 0;
  }
  if ((ctype != CONTROL_TYPE_7BIT  ) &&
      (ctype != CONTROL_TYPE_14BIT ) &&
      (ctype != CONTROL_TYPE_NONREG) &&
      (ctype != CONTROL_TYPE_REG   )) {
    idx = 0;
  }
  
  /* Look for a relevant existing record */
  i = seekMap((int) ctype, (int) ch, (int) idx);
  
  /* If we got a valid index, check whether the index exactly matches
   * the key */
  eq = 0;
  if (i >= 0) {
    if (((m_map[i]).ctype == ctype) &&
        ((m_map[i]).ch    == ch   ) &&
        ((m_map[i]).idx   == idx  )) {
      eq = 1;
    }
  }
  
  /* Handle the cases */
  if (i < 0) {
    /* No relevant record was found, so append a new record */
    capMap(1);
    
    (m_map[m_map_len]).ctype = (int8_t)  ctype;
    (m_map[m_map_len]).ch    = (int8_t)  ch;
    (m_map[m_map_len]).idx   = (int16_t) idx;
    (m_map[m_map_len]).pg    = pg;
    
    m_map_len++;
    
  } else if (eq) {
    /* An existing record already has a matching key, so just update its
     * graph value */
    (m_map[i]).pg = pg;
    
  } else {
    /* A new record is required and insert it before the record at index
     * i -- begin by expanding the array capacity if necessary */
    capMap(1);
    
    /* Shift records to make room for the new record */
    for(j = m_map_len - 1; j >= i; j--) {
      memcpy(&(m_map[j + 1]), &(m_map[j]), sizeof(CTL_MAP));
    }
    m_map_len++;
    
    /* Add the new record */
    (m_map[i]).ctype = (int8_t)  ctype;
    (m_map[i]).ch    = (int8_t)  ch;
    (m_map[i]).idx   = (int16_t) idx;
    (m_map[i]).pg    = pg;
  }
}

/*
 * control_track function.
 */
void control_track(void) {
  
  int32_t track_start = 0;
  int32_t track_end   = 0;
  
  int32_t i = 0;
  
  /* Determine the starting and ending moment offset of the event range
   * where the controller will be tracked */
  track_start = pointer_pack(midi_range_lower(), 0);
  track_end   = pointer_pack(midi_range_upper(), 2);
  
  /* Track all of the mapped graphs */
  for(i = 0; i < m_map_len; i++) {
    graph_track(
      (m_map[i]).pg,
      &trackCtl,
      &(m_map[i]),
      track_start,
      track_end,
      0,
      1,
      0);
  }
}
