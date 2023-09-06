/*
 * render.c
 * ========
 * 
 * Implementation of render.h
 * 
 * See header for further information.
 */

#include "render.h"

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostic.h"
#include "pointer.h"

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
 * The different types of values that can be stored in PIPE_CLASS
 * structures.
 */
#define CLASS_ART         (1)
#define CLASS_RULER       (2)
#define CLASS_GRAPH       (3)
#define CLASS_CHANNEL     (4)
#define CLASS_RELEASE     (5)
#define CLASS_AFTERTOUCH  (6)

/*
 * The initial and maximum capacities of the pipeline.
 */
#define PIPE_INIT_CAP (64)
#define PIPE_MAX_CAP  (16384)

/*
 * Data types
 * ==========
 */

/*
 * Pipeline classifier structure.
 */
typedef struct {
  
  /*
   * Classifier only applies if NMF section is in this set.
   */
  SET *pSect;
  
  /*
   * Classifier only applies if NMF layer is in this set.
   */
  SET *pLayer;
  
  /*
   * Classifier only applies if NMF articulation is in this set.
   */
  SET *pArt;
  
  /*
   * The type of value stored in this classifier.  One of the CLASS
   * constants.
   */
  int ctype;
  
  union {
    /*
     * Articulation value, used for CLASS_ART type.
     */
    ART *pa;
    
    /*
     * Ruler value, used for CLASS_RULER type.
     */
    RULER *pr;
    
    /*
     * Graph value, used for CLASS_GRAPH type.
     */
    GRAPH *pg;
    
    /*
     * Integer value, used for CLASS_CHANNEL, CLASS_RELEASE, and
     * CLASS_AFTERTOUCH types.
     */
    int iv;
    
  } v;
  
} PIPE_CLASS;

/*
 * Structure for holding the results of running the pipeline.
 */
typedef struct {
  
  /*
   * The articulation selected by the pipeline.
   */
  ART *pArt;
  
  /*
   * The ruler selected by the pipeline.
   */
  RULER *pRuler;
  
  /*
   * The graph selected by the pipeline.
   */
  GRAPH *pGraph;
  
  /*
   * The one-indexed MIDI channel selected by the pipeline.
   * 
   * In range 1 to MIDI_CH_MAX inclusive.
   */
  int ch;
  
  /*
   * The release velocity selected by the pipeline.
   * 
   * In range -1 to MIDI_DATA_MAX.  The special value -1 means to use a
   * Note-On message with a velocity of zero.
   */
  int release;
  
  /*
   * The aftertouch enable flag selected by the pipeline.
   * 
   * Either zero or one.
   */
  int after;
  
} PIPE_RESULT;

/*
 * Infrared note event structure.
 */
typedef struct {
  
  /*
   * Event ID.
   * 
   * Each note event gets a unique event ID, assigned in ascending
   * order.
   * 
   * If the event ID is negative, this has the special interpretation
   * that the event has been deleted.
   */
  int32_t eid;
  
  /*
   * The subquantum performance offset of the start of the note.
   * 
   * This might be negative.
   */
  int32_t t;
  
  /*
   * The subquantum performance duration of the note.
   * 
   * This must be greater than zero.
   */
  int32_t dur;
  
  /*
   * The MIDI key number of the note.
   * 
   * This must be in range zero to MIDI_DATA_MAX inclusive.
   */
  uint8_t key;
  
  /*
   * The one-indexed MIDI channel number of the note.
   * 
   * This must be in range 1 to MIDI_CH_MAX inclusive.
   */
  uint8_t ch;
  
  /*
   * The release velocity of the note.
   * 
   * This is either a value in range 0 to MIDI_DATA_MAX inclusive that
   * indicates the velocity to use with the Note-0ff message, or it is
   * the special value -1 which indicates that a Note-On message with a
   * velocity of zero should be used.
   */
  int8_t release;
  
  /*
   * The aftertouch enable flag.
   * 
   * This is either one, enabling polyphonic aftertouch; or zero,
   * disabling polyphonic aftertouch.
   */
  uint8_t after;
  
  /*
   * The graph determining the velocity of the note.
   * 
   * The note-on velocity is always the result of querying the graph at
   * moment offset corresponding to the t field of this event, with a
   * moment part of middle-of-moment.
   * 
   * If the after flag is set for this note, enabling aftertouch, then
   * this graph also determines the polyphonic aftertouch messages that
   * will be issued to get the note to track the graph while the note is
   * active.
   *
   * This graph does NOT determine the release velocity of the note.
   */
  GRAPH *pg;
  
} IR_EVENT;

/*
 * Structure holding state for the graph tracking callback.
 */
typedef struct {
  
  /* The MIDI channel */
  int ch;
  
  /* The MIDI key */
  int key;
  
} AFTER_STATE;

/*
 * Local data
 * ==========
 */

/*
 * Flag indicating whether the render_nmf() function has been called
 * yet.
 */
static int m_render = 0;

/*
 * The pipeline.
 * 
 * Each note is run through the pipeline in sequential order.  For each
 * type of value, the last classifier that applies to the note is
 * selected for that value.  If none of the classifiers apply, the
 * default value is used for that type.
 * 
 * The fields here are the current capacity as a record count, the
 * number of records actually in use, and a pointer to the dynamically
 * allocated record array.
 */
static int32_t m_pipe_cap = 0;
static int32_t m_pipe_len = 0;
static PIPE_CLASS *m_pipe = NULL;

/*
 * The default articulation, ruler, and graph objects used in pipelines,
 * or NULL if they have not been allocated yet.
 */
static ART *m_def_art = NULL;
static RULER *m_def_ruler = NULL;
static GRAPH *m_def_graph = NULL;

/*
 * The event ID assignment variable.
 * 
 * Each time an event ID is assigned, this variable is incremented and
 * the incremented value is the new event ID.  Therefore, the first
 * assigned event ID will be 1.
 */
static int32_t m_unique = 0;

/*
 * The event buffer.
 * 
 * This will be allocated with the same length in event as the number of
 * notes in the parsed NMF data file.  Event structures with negative
 * event IDs in the buffer represent "deleted" structures.
 */
static int32_t m_buf_len = 0;
static IR_EVENT *m_buf = NULL;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static long srcLine(long lnum);

static void capPipe(int32_t n);
static void runPipe(const NMF_NOTE *pn, PIPE_RESULT *pResult);

static int32_t eventID(void);

static void importNotes(NMF_DATA *pd);

static int cmpEvent(const void *pA, const void *pB);
static int overlaps(int32_t t1, int32_t dur, int32_t t2);
static void keyboard(void);

static void afterTrack(void *pCustom, int32_t t, int32_t v);

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
 * Make room in capacity for a given number of elements in the pipeline.
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
static void capPipe(int32_t n) {
  
  int32_t target = 0;
  int32_t new_cap = 0;
  
  /* Check parameters */
  if (n < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Only proceed if n is non-zero */
  if (n > 0) {
    
    /* Make initial allocation if necessary */
    if (m_pipe_cap < 1) {
      m_pipe = (PIPE_CLASS *) calloc(
                  (size_t) PIPE_INIT_CAP, sizeof(PIPE_CLASS));
      if (m_pipe == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      m_pipe_cap = PIPE_INIT_CAP;
      m_pipe_len = 0;
    }
    
    /* Compute target length */
    if (n <= INT32_MAX - m_pipe_len) {
      target = m_pipe_len + n;
    } else {
      raiseErr(__LINE__, "Rendering pipeline capacity exceeded");
    }
    
    /* Only proceed if target length exceeds current capacity */
    if (target > m_pipe_cap) {
      /* Check that target within maximum capacity */
      if (target > PIPE_MAX_CAP) {
        raiseErr(__LINE__, "Rendering pipeline capacity exceeded");
      }
      
      /* Compute new capacity by doubling current capacity until greater
       * than or equal to target length, and then limiting to maximum
       * capacity */
      new_cap = m_pipe_cap;
      while (new_cap < target) {
        new_cap *= 2;
      }
      if (new_cap > PIPE_MAX_CAP) {
        new_cap = PIPE_MAX_CAP;
      }
      
      /* Expand capacity */
      m_pipe = (PIPE_CLASS *) realloc(m_pipe,
                            ((size_t) new_cap) * sizeof(PIPE_CLASS));
      if (m_pipe == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      memset(
        &(m_pipe[m_pipe_cap]),
        0,
        ((size_t) (new_cap - m_pipe_cap)) * sizeof(PIPE_CLASS));
      
      m_pipe_cap = new_cap;
    }
  }
}

/*
 * Run an NMF note through the pipeline to determine rendering
 * information.
 * 
 * Parameters:
 * 
 *   pn - the NMF note to run through the pipeline
 * 
 *   pResult - structure to store the results of the pipeline
 */
static void runPipe(const NMF_NOTE *pn, PIPE_RESULT *pResult) {
  
  int32_t n_art = 0;
  int32_t n_sect = 0;
  int32_t n_layer = 0;
  int32_t i = 0;
  const PIPE_CLASS *pc = NULL;
  
  /* Check parameters */
  if ((pn == NULL) || (pResult == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Clear the result structure and check the input structure's relevant
   * fields, then get the decoded articulation, section, and layer
   * fields */
  memset(pResult, 0, sizeof(PIPE_RESULT));
  
  if (pn->art > NMF_MAXART) {
    raiseErr(__LINE__, NULL);
  }
  
  n_art = (int32_t) pn->art;
  n_sect = (int32_t) pn->sect;
  n_layer = ((int32_t) pn->layer_i) + 1;
  
  /* Allocate default objects if not allocated yet */
  if (m_def_art == NULL) {
    m_def_art = art_new(1, 1, 8, 0, -1);
  }
  if (m_def_ruler == NULL) {
    m_def_ruler = ruler_new(48, 0, -1);
  }
  if (m_def_graph == NULL) {
    m_def_graph = graph_constant(64, -1);
  }
  
  /* Set the defaults in result */
  pResult->pArt = m_def_art;
  pResult->pRuler = m_def_ruler;
  pResult->pGraph = m_def_graph;
  pResult->ch = 1;
  pResult->release = -1;
  pResult->after = 0;
  
  /* Apply classifiers in the pipeline */
  for(i = 0; i < m_pipe_len; i++) {
    pc = &(m_pipe[i]);
    if (set_has(pc->pSect , n_sect ) &&
        set_has(pc->pLayer, n_layer) &&
        set_has(pc->pArt  , n_art  )) {
      
      switch (pc->ctype) {
        case CLASS_ART:
          pResult->pArt = (pc->v).pa;
          break;
        
        case CLASS_RULER:
          pResult->pRuler = (pc->v).pr;
          break;
        
        case CLASS_GRAPH:
          pResult->pGraph = (pc->v).pg;
          break;
        
        case CLASS_CHANNEL:
          pResult->ch = (pc->v).iv;
          break;
        
        case CLASS_RELEASE:
          pResult->release = (pc->v).iv;
          break;
        
        case CLASS_AFTERTOUCH:
          pResult->after = (pc->v).iv;
          break;
        
        default:
          raiseErr(__LINE__, NULL);
      }
    }
  }
}

/*
 * Return a newly generated event ID.
 * 
 * The first generated event ID is 1 and all subsequent calls are one
 * greater than the previous return value.  An error occurs if the range
 * of the 32-bit integer would be exceeded.
 * 
 * Return:
 * 
 *   a new event ID
 */
static int32_t eventID(void) {
  if (m_unique < INT32_MAX) {
    m_unique++;
  } else {
    raiseErr(__LINE__, "Event ID generation overflow");
  }
  return m_unique;
}

/*
 * Import all the notes from a parsed NMF data object into a buffer of
 * Infrared events.
 * 
 * The pipeline should be set up, as it will be used to get performance
 * data for the notes.  The buffer must not be allocated yet or an error
 * occurs.
 * 
 * The result will be held in the m_buf buffer, with length given by
 * m_buf_len.  Structures that have negative event IDs should be assumed
 * deleted.
 * 
 * Parameters:
 * 
 *   pd - the NMF data to import
 */
static void importNotes(NMF_DATA *pd) {
  
  int32_t i = 0;
  IR_EVENT *pe = NULL;
  NMF_NOTE ns;
  PIPE_RESULT r;
  
  /* Initialize structures */
  memset(&ns, 0, sizeof(NMF_NOTE));
  memset(&r, 0, sizeof(PIPE_RESULT));
  
  /* Check state and parameters */
  if (m_buf_len > 0) {
    raiseErr(__LINE__, NULL);
  }
  if (pd == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Only proceed if at least one note */
  if (nmf_notes(pd) > 0) {
    
    /* Allocate buffer with same length as the number of notes in the
     * NMF data */
    m_buf_len = nmf_notes(pd);
    m_buf = (IR_EVENT *) calloc((size_t) m_buf_len, sizeof(IR_EVENT));
    if (m_buf == NULL) {
      raiseErr(__LINE__, NULL);
    }
    
    /* Import each NMF note */
    for(i = 0; i < m_buf_len; i++) {
      /* Get the NMF note and the Infrared event */
      nmf_get(pd, i, &ns);
      pe = &(m_buf[i]);
      
      /* If NMF duration is zero, then delete the corresponding Infrared
       * event by setting a negative event ID and skip reset of
       * processing; else, assign a unique event ID */
      if (ns.dur == 0) {
        pe->eid = -1;
        continue;
      }
      pe->eid = eventID();
      
      /* Use the pipeline to classify this note */
      runPipe(&ns, &r);
      
      /* Determine performance offset and duration based on whether note
       * is measured or unmeasured */
      if (ns.dur > 0) {
        /* Measured note, so determine performance offset by multiplying
         * NMF offset by 8 */
        if (ns.t < 0) {
          raiseErr(__LINE__, NULL);
        }
        if (ns.t <= INT32_MAX / 8) {
          pe->t = ns.t * 8;
        } else {
          raiseErr(__LINE__, "Subquantum offset overflow");
        }
        
        /* Use the articulation object assigned to this note by the
         * pipeline to compute the performance duration */
        pe->dur = art_transform(r.pArt, ns.dur);
        
      } else if (ns.dur < 0) {
        /* Unmeasured grace note, so determine performance offset by
         * multiplying NMF offset by 8 and then applying the ruler */
        if (ns.t < 0) {
          raiseErr(__LINE__, NULL);
        }
        if (ns.t <= INT32_MAX / 8) {
          pe->t = ns.t * 8;
        } else {
          raiseErr(__LINE__, "Subquantum offset overflow");
        }
        pe->t = ruler_pos(r.pRuler, pe->t, ns.dur);
        
        /* Take the performance duration from the ruler */
        pe->dur = ruler_dur(r.pRuler);
        
      } else {
        raiseErr(__LINE__, NULL);
      }
      
      /* Determine MIDI key number by adding 60 to the NMF pitch, since
       * MIDI key 60 is middle C, which is NMF pitch zero; if the NMF
       * pitch is in range, the MIDI key number will be in range too */
      if ((ns.pitch < NMF_MINPITCH) || (ns.pitch > NMF_MAXPITCH)) {
        raiseErr(__LINE__, NULL);
      }
      pe->key = (uint8_t) (ns.pitch + 60);
      
      /* The rest of the note data comes from the pipeline */
      pe->ch = (uint8_t) r.ch;
      pe->release = (int8_t) r.release;
      pe->after = (uint8_t) r.after;
      pe->pg = r.pGraph;
    }
  }
}

/*
 * Comparison function for sorting Infrared events to prepare for the
 * keyboard process.
 * 
 * The interface of this function matches the callback function of the
 * standard library qsort().  Both elements should be IR_EVENT
 * structures.
 * 
 * The first comparison is to sort all the events that have non-negative
 * event IDs before all the events that have negative event IDs.  This
 * ensures that all the "deleted" structures with negative event IDs are
 * at the back of the array.
 * 
 * Structures that have negative event IDs always compare as equal to
 * other structures that have negative event IDs, since there is no
 * difference between these "deleted" structures.
 * 
 * The second comparison is by MIDI channel.
 * 
 * The third comparison is by MIDI key.
 * 
 * The fourth comparison is by performance time offset.
 * 
 * The fifth comparison is by performance duration in descending order.
 * 
 * The sixth comparison is by event ID in descending order.
 * 
 * Parameters:
 * 
 *   pA - pointer to first element
 * 
 *   pB - pointer to second element
 * 
 * Return:
 * 
 *   less than zero, zero, or greater than zero as the first element is
 *   less than, equal to, or greater than the second element
 */
static int cmpEvent(const void *pA, const void *pB) {
  
  int result = 0;
  const IR_EVENT *e1 = NULL;
  const IR_EVENT *e2 = NULL;
  
  if ((pA == NULL) || (pB == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  e1 = (const IR_EVENT *) pA;
  e2 = (const IR_EVENT *) pB;
  
  if ((e1->eid < 0) && (e2->eid >= 0)) {
    result = 1;
  } else if ((e1->eid >= 0) && (e2->eid < 0)) {
    result = -1;
  }
  
  if ((result == 0) && (e1->eid >= 0)) {
    
    if (e1->ch < e2->ch) {
      result = -1;
    } else if (e1->ch > e2->ch) {
      result = 1;
    }
    
    if (result == 0) {
      if (e1->key < e2->key) {
        result = -1;
      } else if (e1->key > e2->key) {
        result = 1;
      }
    }
    
    if (result == 0) {
      if (e1->t < e2->t) {
        result = -1;
      } else if (e1->t > e2->t) {
        result = 1;
      }
    }
    
    if (result == 0) {
      if (e1->dur > e2->dur) {
        result = -1;
      } else if (e1->dur < e2->dur) {
        result = 1;
      }
    }
    
    if (result == 0) {
      if (e1->eid > e2->eid) {
        result = -1;
      } else if (e1->eid < e2->eid) {
        result = 1;
      }
    }
  }
  
  return result;
}

/*
 * Check whether one event overlaps with another.
 * 
 * t1 is the time offset of the first event.  dur is the duration of the
 * first event, which must be greater than zero.  t2 is the time offset
 * of the second event, which must be greater than t1.
 * 
 * Non-zero is returned only if (t1+dur) > t2.  This function performs
 * the calculation safely in 64-bit space to avoid potential overflow
 * problems.
 * 
 * Parameters:
 * 
 *   t1 - the time offset of the first event
 * 
 *   dur - the duration of the first event
 * 
 *   t2 - the time offset of the second event
 * 
 * Return:
 * 
 *   non-zero if first event overlaps second, zero otherwise
 */
static int overlaps(int32_t t1, int32_t dur, int32_t t2) {
  
  int result = 0;
  int32_t te = 0;
  
  if (dur < 1) {
    raiseErr(__LINE__, NULL);
  }
  if (t2 <= t1) {
    raiseErr(__LINE__, NULL);
  }
  
  if (t1 <= INT32_MAX - dur) {
    te = t1 + dur;
    if (te > t2) {
      result = 1;
    } else {
      result = 0;
    }
    
  } else {
    result = 0;
  }
  
  return result;
}

/*
 * Perform the "keyboard process."
 * 
 * This function operates on the event buffer.  The event buffer should
 * already be filled with importNotes().  If this function is run on an
 * empty event buffer, it does nothing.
 * 
 * The function begins by sorting the event buffer (if it has at least
 * two events) using cmpEvent() as the comparison function.  This will
 * put all "deleted" events at the end of the buffer.  The other events
 * will be sorted first by MIDI channel, then by MIDI key, then by time
 * offset, then by descending duration, and then by descending event ID.
 * 
 * For each sorted sequence of events that share the same channel, key,
 * and time offset, the first event in the sequence is retained and all
 * other events are "deleted" by setting their event ID to a negative
 * value.  This means that when there are multiple events starting on
 * the same channel on the same key at the same time, only the longest
 * event is chosen.  If there are multiple longest events, the latest
 * defined event (as determined by event ID) is chosen.
 * 
 * The first event in each sequence (including sequences of only one
 * event) is then compared to the first event in the next sequence (if
 * there is a next sequence).  If the next sequence has the same MIDI
 * channel and MIDI key, then the duration of the current sequence event
 * is shortened if necessary such that its release is no later than the
 * onset of the next sequence.
 * 
 * This keyboard process guarantees that within each key of each MIDI
 * channel, there will be no overlapping events.
 */
static void keyboard(void) {
  
  int32_t i = 0;
  int32_t j = 0;
  
  /* Only proceed if at least two events */
  if (m_buf_len > 1) {
    
    /* Sort all the events */
    qsort(m_buf, (size_t) m_buf_len, sizeof(IR_EVENT), &cmpEvent);
    
    /* Iterate through all events */
    for(i = 0; i < m_buf_len; i++) {
    
      /* If this event is "deleted" then all the rest are deleted too so
       * leave loop */
      if ((m_buf[i]).eid < 0) {
        break;
      }
      
      /* Iterate through all events after this one */
      for(j = i + 1; j < m_buf_len; j++) {
        /* If this event is "deleted" then all the rest are deleted too
         * so leave inner loop */
        if ((m_buf[j]).eid < 0) {
          break;
        }
        
        /* If this event does not have the same MIDI channel and MIDI 
         * key as the outer loop, then leave loop */
        if (((m_buf[j]).ch  != (m_buf[i]).ch ) ||
            ((m_buf[j]).key != (m_buf[i]).key)) {
          break;
        }
        
        /* If we got here, inner loop has same MIDI channel and MIDI
         * key; check if time offset is also the same */
        if ((m_buf[j]).t == (m_buf[i]).t) {
          /* Same MIDI channel, MIDI key, and time offset, so delete
           * this inner loop event */
          (m_buf[j]).eid = -1;
          
        } else {
          /* Same MIDI channel and MIDI key, but different time offset;
           * shorten outer loop event if necessary to prevent overlap */
          if (overlaps((m_buf[i]).t, (m_buf[i]).dur, (m_buf[j]).t)) {
            (m_buf[i]).dur = (m_buf[j]).t - (m_buf[i]).t;
          }
          
          /* Decrement j so this next event will be processed again in
           * the outer loop; then, leave this inner loop */
          j--;
          break;
        }
      }
      
      /* Advance i if necessary so it's just before j and next iteration
       * will be index j */
      i = j - 1;
    }
  }
}

/*
 * Aftertouch graph tracking callback.
 * 
 * This function interface matches the graph_fp_callback function
 * pointer interface.
 * 
 * The custom data must be an AFTER_STATE structure.
 * 
 * Parameters:
 * 
 *   pCustom - the AFTER_STATE structure
 * 
 *   t - the moment offset of a change in value
 * 
 *   v - the new value being changed to
 */
static void afterTrack(void *pCustom, int32_t t, int32_t v) {
  
  const AFTER_STATE *ps = NULL;
  
  /* Check parameters */
  if (pCustom == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if ((v < 1) || (v > MIDI_DATA_MAX)) {
    raiseErr(__LINE__, "Aftertouch graph value out of range");
  }
  
  /* Get state */
  ps = (const AFTER_STATE *) pCustom;
  
  /* Record the aftertouch message */
  midi_message(
    t,
    0,
    ps->ch,
    MIDI_MSG_POLY_AFTERTOUCH,
    ps->key,
    (int) v);
}

/*
 * Public function implementations
 * ===============================
 * 
 * See header for specifications.
 */

/*
 * render_classify_art function.
 */
void render_classify_art(
    SET * pSect,
    SET * pLayer,
    SET * pArt,
    ART * pVal) {
  
  if (m_render) {
    raiseErr(__LINE__, "Render function already invoked");
  }
  if ((pSect == NULL) || (pLayer == NULL) || (pArt == NULL)
      || (pVal == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  capPipe(1);
  
  (m_pipe[m_pipe_len]).pSect  = pSect;
  (m_pipe[m_pipe_len]).pLayer = pLayer;
  (m_pipe[m_pipe_len]).pArt   = pArt;
  
  (m_pipe[m_pipe_len]).ctype  = CLASS_ART;
  (m_pipe[m_pipe_len]).v.pa   = pVal;
  
  m_pipe_len++;
}

/*
 * render_classify_ruler function.
 */
void render_classify_ruler(
    SET   * pSect,
    SET   * pLayer,
    SET   * pArt,
    RULER * pVal) {
  
  if (m_render) {
    raiseErr(__LINE__, "Render function already invoked");
  }
  if ((pSect == NULL) || (pLayer == NULL) || (pArt == NULL)
      || (pVal == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  capPipe(1);
  
  (m_pipe[m_pipe_len]).pSect  = pSect;
  (m_pipe[m_pipe_len]).pLayer = pLayer;
  (m_pipe[m_pipe_len]).pArt   = pArt;
  
  (m_pipe[m_pipe_len]).ctype  = CLASS_RULER;
  (m_pipe[m_pipe_len]).v.pr   = pVal;
  
  m_pipe_len++;
}

/*
 * render_classify_graph function.
 */
void render_classify_graph(
    SET   * pSect,
    SET   * pLayer,
    SET   * pArt,
    GRAPH * pVal) {
  
  if (m_render) {
    raiseErr(__LINE__, "Render function already invoked");
  }
  if ((pSect == NULL) || (pLayer == NULL) || (pArt == NULL)
      || (pVal == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  capPipe(1);
  
  (m_pipe[m_pipe_len]).pSect  = pSect;
  (m_pipe[m_pipe_len]).pLayer = pLayer;
  (m_pipe[m_pipe_len]).pArt   = pArt;
  
  (m_pipe[m_pipe_len]).ctype  = CLASS_GRAPH;
  (m_pipe[m_pipe_len]).v.pg   = pVal;
  
  m_pipe_len++;
}

/*
 * render_classify_channel function.
 */
void render_classify_channel(
    SET     * pSect,
    SET     * pLayer,
    SET     * pArt,
    int32_t   val,
    long      lnum) {
  
  if (m_render) {
    raiseErr(__LINE__, "Render function already invoked");
  }
  if ((pSect == NULL) || (pLayer == NULL) || (pArt == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  if ((val < 1) || (val > MIDI_CH_MAX)) {
    raiseErr(__LINE__, "Invalid MIDI channel value on script line %ld",
      srcLine(lnum));
  }
  
  capPipe(1);
  
  (m_pipe[m_pipe_len]).pSect  = pSect;
  (m_pipe[m_pipe_len]).pLayer = pLayer;
  (m_pipe[m_pipe_len]).pArt   = pArt;
  
  (m_pipe[m_pipe_len]).ctype  = CLASS_CHANNEL;
  (m_pipe[m_pipe_len]).v.iv   = (int) val;
  
  m_pipe_len++;
}

/*
 * render_classify_release function.
 */
void render_classify_release(
    SET     * pSect,
    SET     * pLayer,
    SET     * pArt,
    int32_t   val,
    long      lnum) {
  
  if (m_render) {
    raiseErr(__LINE__, "Render function already invoked");
  }
  if ((pSect == NULL) || (pLayer == NULL) || (pArt == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  if ((val < -1) || (val > MIDI_DATA_MAX)) {
    raiseErr(__LINE__,
      "Invalid MIDI release velocity on script line %ld",
      srcLine(lnum));
  }
  
  capPipe(1);
  
  (m_pipe[m_pipe_len]).pSect  = pSect;
  (m_pipe[m_pipe_len]).pLayer = pLayer;
  (m_pipe[m_pipe_len]).pArt   = pArt;
  
  (m_pipe[m_pipe_len]).ctype  = CLASS_RELEASE;
  (m_pipe[m_pipe_len]).v.iv   = (int) val;
  
  m_pipe_len++;
}

/*
 * render_classify_aftertouch function.
 */
void render_classify_aftertouch(
    SET     * pSect,
    SET     * pLayer,
    SET     * pArt,
    int32_t   val,
    long      lnum) {
  
  if (m_render) {
    raiseErr(__LINE__, "Render function already invoked");
  }
  if ((pSect == NULL) || (pLayer == NULL) || (pArt == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  if ((val != 0) && (val != 1)) {
    raiseErr(__LINE__,
      "Invalid MIDI aftertouch flag on script line %ld",
      srcLine(lnum));
  }
  
  capPipe(1);
  
  (m_pipe[m_pipe_len]).pSect  = pSect;
  (m_pipe[m_pipe_len]).pLayer = pLayer;
  (m_pipe[m_pipe_len]).pArt   = pArt;
  
  (m_pipe[m_pipe_len]).ctype  = CLASS_AFTERTOUCH;
  (m_pipe[m_pipe_len]).v.iv   = (int) val;
  
  m_pipe_len++;
}

/*
 * render_nmf function.
 */
void render_nmf(NMF_DATA *pd) {
  
  int32_t i = 0;
  int32_t t = 0;
  int32_t t_end = 0;
  int32_t v = 0;
  const IR_EVENT *pe = NULL;
  AFTER_STATE as;
  
  /* Initialize structures */
  memset(&as, 0, sizeof(AFTER_STATE));
  
  /* Check state and update render flag */
  if (m_render) {
    raiseErr(__LINE__, "Render function already invoked");
  }
  m_render = 1;
  
  /* Check parameters */
  if (pd == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Import all NMF notes into Infrared events, and then perform the
   * keyboard process to remove invalid overlap */
  importNotes(pd);
  /* keyboard(); */

  /* Render each Infrared event into MIDI messages */
  for(i = 0; i < m_buf_len; i++) {
    /* Get event */
    pe = &(m_buf[i]);
    
    /* Skip this event if it is deleted */
    if (pe->eid < 0) {
      continue;
    }
    
    /* Get the moment offset of the start of the note, using middle of
     * moment part */
    t = pointer_pack(pe->t, 1);
    
    /* Get the velocity of the note at this moment and check its
     * range */
    v = graph_query(pe->pg, t);
    if ((v < 1) || (v > MIDI_DATA_MAX)) {
      raiseErr(__LINE__, "Note velocity graph out of range");
    }
    
    /* Add the note-on message */
    midi_message(
      t, 0,
      (int) pe->ch,
      MIDI_MSG_NOTE_ON,
      (int) pe->key,
      (int) v);
    
    /* Compute the ending moment offset, using start of moment part */
    if (pe->t <= INT32_MAX - pe->dur) {
      t_end = pe->t + pe->dur;
    } else {
      raiseErr(__LINE__, "Moment offset overflow");
    }
    
    t_end = pointer_pack(t_end, 0);
    if (t_end <= t) {
      raiseErr(__LINE__, NULL);
    }
    
    /* Add the note-off message */
    if (pe->release < 0) {
      midi_message(
        t_end, 0,
        (int) pe->ch,
        MIDI_MSG_NOTE_ON,
        (int) pe->key,
        0);
      
    } else {
      midi_message(
        t_end, 0,
        (int) pe->ch,
        MIDI_MSG_NOTE_OFF,
        (int) pe->key,
        (int) pe->release);
    }
    
    /* If aftertouch is enabled AND the duration in subquanta is at
     * least two, generate necessary aftertouch messages for all
     * subquanta between the first and last */
    if ((pe->after) && (pe->dur >= 2)) {
      /* Compute the time range for aftertouch messages */
      t     = pointer_pack(pe->t           + 1, 0);
      t_end = pointer_pack(pe->t + pe->dur - 1, 2);
      
      /* Initialize callback state */
      as.ch  = (int) pe->ch;
      as.key = (int) pe->key;
      
      /* Track graph changes to generate aftertouch messages */
      graph_track(
        pe->pg,
        &afterTrack,
        &as,
        t,
        t_end,
        v,
        1,
        1);
    }
  }
}
