/*
 * op_render.c
 * ===========
 * 
 * Implementation of op_render.h
 * 
 * See the header for further information.  See the Infrared operations
 * documentation for specifications of the operations.
 */

#include "op_render.h"

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "art.h"
#include "core.h"
#include "diagnostic.h"
#include "graph.h"
#include "main.h"
#include "render.h"
#include "ruler.h"
#include "set.h"

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

#define NOTE_ART (1)
#define NOTE_RULER (2)
#define NOTE_GRAPH (3)
#define NOTE_CHANNEL (4)
#define NOTE_RELEASE (5)
#define AFTERTOUCH_ENABLE (6)
#define AFTERTOUCH_DISABLE (7)

/*
 * Local data
 * ==========
 */

static int i_note_art = NOTE_ART;
static int i_note_ruler = NOTE_RULER;
static int i_note_graph = NOTE_GRAPH;
static int i_note_channel = NOTE_CHANNEL;
static int i_note_release = NOTE_RELEASE;
static int i_aftertouch_enable = AFTERTOUCH_ENABLE;
static int i_aftertouch_disable = AFTERTOUCH_DISABLE;

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

/*
 * pCustom points to an int holding one of the constants defined earlier
 * indicating the specific classifier that is being registered.
 */
static void op_note_class(void *pCustom, long lnum) {
  
  CORE_VARIANT cv;
  int *pt = NULL;
  SET *sSect = NULL;
  SET *sLayer = NULL;
  SET *sArt = NULL;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  
  if (pCustom == NULL) {
    raiseErr(__LINE__, NULL);
  }
  pt = (int *) pCustom;
  
  if (*pt == AFTERTOUCH_ENABLE) {
    cv.tcode = (uint8_t) CORE_T_INTEGER;
    cv.v.iv = 1;
    
  } else if (*pt == AFTERTOUCH_DISABLE) {
    cv.tcode = (uint8_t) CORE_T_INTEGER;
    cv.v.iv = 0;
    
  } else {
    core_pop(&cv, lnum);
  }
  
  sArt = core_pop_s(lnum);
  sLayer = core_pop_s(lnum);
  sSect = core_pop_s(lnum);
  
  if (*pt == NOTE_ART) {
    if (cv.tcode != CORE_T_ART) {
      raiseErr(__LINE__, "Expecting articulation on script line %ld",
        srcLine(lnum));
    }
    render_classify_art(sSect, sLayer, sArt, cv.v.pArt);
    
  } else if (*pt == NOTE_RULER) {
    if (cv.tcode != CORE_T_RULER) {
      raiseErr(__LINE__, "Expecting ruler on script line %ld",
        srcLine(lnum));
    }
    render_classify_ruler(sSect, sLayer, sArt, cv.v.pRuler);
    
  } else if (*pt == NOTE_GRAPH) {
    if (cv.tcode != CORE_T_GRAPH) {
      raiseErr(__LINE__, "Expecting graph on script line %ld",
        srcLine(lnum));
    }
    render_classify_graph(sSect, sLayer, sArt, cv.v.pGraph);
    
  } else if (*pt == NOTE_CHANNEL) {
    if (cv.tcode != CORE_T_INTEGER) {
      raiseErr(__LINE__, "Expecting integer on script line %ld",
        srcLine(lnum));
    }
    render_classify_channel(sSect, sLayer, sArt, cv.v.iv, lnum);
    
  } else if (*pt == NOTE_RELEASE) {
    if (cv.tcode != CORE_T_INTEGER) {
      raiseErr(__LINE__, "Expecting integer on script line %ld",
        srcLine(lnum));
    }
    render_classify_release(sSect, sLayer, sArt, cv.v.iv, lnum);
    
  } else if ((*pt == AFTERTOUCH_ENABLE) ||
              (*pt == AFTERTOUCH_DISABLE)) {
    if (cv.tcode != CORE_T_INTEGER) {
      raiseErr(__LINE__, NULL);
    }
    render_classify_aftertouch(sSect, sLayer, sArt, cv.v.iv, lnum);
    
  } else {
    raiseErr(__LINE__, NULL);
  }
}

/*
 * Registration function
 * =====================
 */

void op_render_register(void) {
  main_op("note_art", &op_note_class, &i_note_art);
  main_op("note_ruler", &op_note_class, &i_note_ruler);
  main_op("note_graph", &op_note_class, &i_note_graph);
  main_op("note_channel", &op_note_class, &i_note_channel);
  main_op("note_release", &op_note_class, &i_note_release);
  main_op("aftertouch_enable", &op_note_class, &i_aftertouch_enable);
  main_op("aftertouch_disable", &op_note_class, &i_aftertouch_disable);
}
