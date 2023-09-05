/*
 * op_control.c
 * ============
 * 
 * Implementation of op_control.h
 * 
 * See the header for further information.  See the Infrared operations
 * documentation for specifications of the operations.
 */

#include "op_control.h"

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "blob.h"
#include "control.h"
#include "core.h"
#include "diagnostic.h"
#include "graph.h"
#include "main.h"
#include "midi.h"
#include "pointer.h"
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
 * Local data
 * ==========
 */

static int i_general = MIDI_TEXT_GENERAL;
static int i_copyright = MIDI_TEXT_COPYRIGHT;
static int i_title = MIDI_TEXT_TITLE;
static int i_instrument = MIDI_TEXT_INSTRUMENT;
static int i_lyric = MIDI_TEXT_LYRIC;
static int i_marker = MIDI_TEXT_MARKER;
static int i_cue = MIDI_TEXT_CUE;

static int i_minor = 1;
static int i_major = 0;

static int i_sound_off = CONTROL_MODE_SOUND_OFF;
static int i_reset = CONTROL_MODE_RESET;
static int i_local_off = CONTROL_MODE_LOCAL_OFF;
static int i_local_on = CONTROL_MODE_LOCAL_ON;
static int i_notes_off = CONTROL_MODE_NOTES_OFF;
static int i_omni_off = CONTROL_MODE_OMNI_OFF;
static int i_omni_on = CONTROL_MODE_OMNI_ON;
static int i_poly = CONTROL_MODE_POLY;

static int i_7bit = CONTROL_TYPE_7BIT;
static int i_14bit = CONTROL_TYPE_14BIT;
static int i_nonreg = CONTROL_TYPE_NONREG;
static int i_reg = CONTROL_TYPE_REG;
static int i_pressure = CONTROL_TYPE_PRESSURE;
static int i_pitch = CONTROL_TYPE_PITCH;

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

static void op_null_event(void *pCustom, long lnum) {
  POINTER *pp = NULL;
  
  (void) pCustom;
  
  pp = core_pop_p(lnum);
  control_null(pp, lnum);
}

/*
 * pCustom points to an int holding the text class.
 */
static void op_text(void *pCustom, long lnum) {
  
  int *pt = NULL;
  POINTER *pp = NULL;
  TEXT *pText = NULL;
  
  if (pCustom == NULL) {
    raiseErr(__LINE__, NULL);
  }
  pt = (int *) pCustom;
  
  pText = core_pop_t(lnum);
  pp = core_pop_p(lnum);
  
  control_text(pp, *pt, pText, lnum);
}

static void op_time_sig(void *pCustom, long lnum) {
  
  POINTER *pp = NULL;
  int32_t num = 0;
  int32_t denom = 0;
  int32_t metro = 0;
  
  (void) pCustom;
  
  metro = core_pop_i(lnum);
  denom = core_pop_i(lnum);
  num = core_pop_i(lnum);
  pp = core_pop_p(lnum);
  
  control_time_sig(pp, num, denom, metro, lnum);
}

/*
 * pCustom points to an int that is one for minor key, zero for major
 * key.
 */
static void op_key(void *pCustom, long lnum) {
  
  int *pt = NULL;
  POINTER *pp = NULL;
  int32_t count = 0;
  
  if (pCustom == NULL) {
    raiseErr(__LINE__, NULL);
  }
  pt = (int *) pCustom;
  
  count = core_pop_i(lnum);
  pp = core_pop_p(lnum);
  
  control_key_sig(pp, count, (int32_t) *pt, lnum);
}

static void op_custom(void *pCustom, long lnum) {
  POINTER *pp = NULL;
  BLOB *pb = NULL;
  
  (void) pCustom;
  
  pb = core_pop_b(lnum);
  pp = core_pop_p(lnum);
  
  control_custom(pp, pb, lnum);
}

static void op_sysex(void *pCustom, long lnum) {
  POINTER *pp = NULL;
  BLOB *pb = NULL;
  
  (void) pCustom;
  
  pb = core_pop_b(lnum);
  pp = core_pop_p(lnum);
  
  control_system(pp, pb, lnum);
}

static void op_program(void *pCustom, long lnum) {
  POINTER *pp = NULL;
  int32_t ch = 0;
  int32_t v = 0;
  
  (void) pCustom;
  
  v = core_pop_i(lnum);
  ch = core_pop_i(lnum);
  pp = core_pop_p(lnum);
  
  control_instrument(pp, ch, 0, v, 0, lnum);
}

static void op_patch(void *pCustom, long lnum) {
  POINTER *pp = NULL;
  int32_t ch = 0;
  int32_t b = 0;
  int32_t v = 0;
  
  (void) pCustom;
  
  v = core_pop_i(lnum);
  b = core_pop_i(lnum);
  ch = core_pop_i(lnum);
  pp = core_pop_p(lnum);
  
  control_instrument(pp, ch, b, v, 1, lnum);
}

/*
 * pCustom points to an int that has the modal message type.  This
 * function should not be used for the mono message, which requires an
 * extra parameter.
 */
static void op_modal(void *pCustom, long lnum) {
  int *pt = NULL;
  POINTER *pp = NULL;
  int32_t ch = 0;
  
  if (pCustom == NULL) {
    raiseErr(__LINE__, NULL);
  }
  pt = (int *) pCustom;
  
  ch = core_pop_i(lnum);
  pp = core_pop_p(lnum);
  
  control_modal(pp, ch, *pt, 0, lnum);
}

static void op_mono(void *pCustom, long lnum) {
  POINTER *pp = NULL;
  int32_t ch = 0;
  int32_t count = 0;
  
  (void) pCustom;
  
  count = core_pop_i(lnum);
  ch = core_pop_i(lnum);
  pp = core_pop_p(lnum);
  
  control_modal(pp, ch, CONTROL_MODE_MONO, count, lnum);
}

static void op_auto_tempo(void *pCustom, long lnum) {
  GRAPH *pg = NULL;
  
  (void) pCustom;
  
  pg = core_pop_g(lnum);
  
  control_auto(CONTROL_TYPE_TEMPO, 0, 0, pg, lnum);
}

/*
 * pCustom points to an int that has the controller type.  This function
 * is used for controllers that include both a channel and an index.
 */
static void op_auto_idx(void *pCustom, long lnum) {
  
  int *pt = NULL;
  int32_t ch = 0;
  int32_t idx = 0;
  GRAPH *pg = NULL;
  
  if (pCustom == NULL) {
    raiseErr(__LINE__, NULL);
  }
  pt = (int *) pCustom;
  
  pg = core_pop_g(lnum);
  idx = core_pop_i(lnum);
  ch = core_pop_i(lnum);
  
  control_auto(*pt, ch, idx, pg, lnum);
}

/*
 * pCustom points to an int that has the controller type.  This function
 * is used for controllers that have a channel but not an index.
 */
static void op_auto_ch(void *pCustom, long lnum) {
  
  int *pt = NULL;
  int32_t ch = 0;
  GRAPH *pg = NULL;
  
  if (pCustom == NULL) {
    raiseErr(__LINE__, NULL);
  }
  pt = (int *) pCustom;
  
  pg = core_pop_g(lnum);
  ch = core_pop_i(lnum);
  
  control_auto(*pt, ch, 0, pg, lnum);
}

/*
 * Registration function
 * =====================
 */

void op_control_register(void) {
  main_op("null_event", &op_null_event, NULL);
  
  main_op("text", &op_text, &i_general);
  main_op("text_copyright", &op_text, &i_copyright);
  main_op("text_title", &op_text, &i_title);
  main_op("text_instrument", &op_text, &i_instrument);
  main_op("text_lyric", &op_text, &i_lyric);
  main_op("text_marker", &op_text, &i_marker);
  main_op("text_cue", &op_text, &i_cue);
  
  main_op("time_sig", &op_time_sig, NULL);
  
  main_op("major_key", &op_key, &i_major);
  main_op("minor_key", &op_key, &i_minor);
  
  main_op("custom", &op_custom, NULL);
  main_op("sysex", &op_sysex, NULL);
  main_op("program", &op_program, NULL);
  main_op("patch", &op_patch, NULL);
  
  main_op("sound_off", &op_modal, &i_sound_off);
  main_op("midi_reset", &op_modal, &i_reset);
  main_op("local_off", &op_modal, &i_local_off);
  main_op("local_on", &op_modal, &i_local_on);
  main_op("notes_off", &op_modal, &i_notes_off);
  main_op("omni_off", &op_modal, &i_omni_off);
  main_op("omni_on", &op_modal, &i_omni_on);
  main_op("mono", &op_mono, NULL);
  main_op("poly", &op_modal, &i_poly);
  
  main_op("auto_tempo", &op_auto_tempo, NULL);
  main_op("auto_7bit", &op_auto_idx, &i_7bit);
  main_op("auto_14bit", &op_auto_idx, &i_14bit);
  main_op("auto_nonreg", &op_auto_idx, &i_nonreg);
  main_op("auto_reg", &op_auto_idx, &i_reg);
  main_op("auto_pressure", &op_auto_ch, &i_pressure);
  main_op("auto_pitch", &op_auto_ch, &i_pitch);
}
