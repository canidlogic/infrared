/*
 * op_base.c
 * =========
 * 
 * Implementation of op_base.h
 * 
 * See the header for further information.  See the Infrared operations
 * documentation for specifications of the operations.
 */

#include "op_base.h"

#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "diagnostic.h"
#include "main.h"
#include "midi.h"
#include "pointer.h"
#include "primitive.h"
#include "ruler.h"

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

#define OP_ADD (1)
#define OP_SUB (2)
#define OP_MUL (3)
#define OP_DIV (4)

#define OP_S (5)
#define OP_Q (6)
#define OP_R (7)
#define OP_G (8)
#define OP_T (9)
#define OP_M (10)

/*
 * Local data
 * ==========
 */

static int i_add = OP_ADD;
static int i_sub = OP_SUB;
static int i_mul = OP_MUL;
static int i_div = OP_DIV;

static int i_s = OP_S;
static int i_q = OP_Q;
static int i_r = OP_R;
static int i_g = OP_G;
static int i_t = OP_T;
static int i_m = OP_M;

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

static void op_pop(void *pCustom, long lnum) {
  CORE_VARIANT cv;
  
  (void) pCustom;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  core_pop(&cv, lnum);
}

static void op_dup(void *pCustom, long lnum) {
  CORE_VARIANT cv;
  
  (void) pCustom;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  core_pop(&cv, lnum);
  core_push(&cv, lnum);
  core_push(&cv, lnum);
}

static void op_print(void *pCustom, long lnum) {
  CORE_VARIANT cv;
  
  (void) pCustom;
  
  memset(&cv, 0, sizeof(CORE_VARIANT));
  core_pop(&cv, lnum);
  main_print(&cv, lnum);
}

static void op_newline(void *pCustom, long lnum) {
  (void) pCustom;
  (void) lnum;
  
  main_newline();
}

static void op_stop(void *pCustom, long lnum) {
  (void) pCustom;
  
  main_stop(lnum);
}

/*
 * pCustom points to an int selecting the operation to perform, which
 * must be one of the OP_ constants.
 */
static void op_binary(void *pCustom, long lnum) {
  int32_t a = 0;
  int32_t b = 0;
  int32_t c = 0;
  int *po = NULL;
  
  if (pCustom == NULL) {
    raiseErr(__LINE__, NULL);
  }
  po = (int *) pCustom;
  
  b = core_pop_i(lnum);
  a = core_pop_i(lnum);
  
  if (*po == OP_ADD) {
    c = primitive_add(a, b, lnum);
    
  } else if (*po == OP_SUB) {
    c = primitive_sub(a, b, lnum);
    
  } else if (*po == OP_MUL) {
    c = primitive_mul(a, b, lnum);
    
  } else if (*po == OP_DIV) {
    c = primitive_div(a, b, lnum);
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  core_push_i(c, lnum);
}

static void op_neg(void *pCustom, long lnum) {
  int32_t iv = 0;
  
  (void) pCustom;
  
  iv = core_pop_i(lnum);
  iv = 0 - iv;
  core_push_i(iv, lnum);
}

/*
 * pCustom points to an int which has the specific pointer arithmetic
 * operation to perform.
 */
static void op_ptr_math(void *pCustom, long lnum) {
  
  int *pt = NULL;
  POINTER *pp = NULL;
  int32_t i = 0;
  
  if (pCustom == NULL) {
    raiseErr(__LINE__, NULL);
  }
  pt = (int *) pCustom;
  
  i = core_pop_i(lnum);
  pp = core_pop_p(lnum);
  
  if (*pt == OP_S) {
    pointer_jump(pp, i, lnum);
    
  } else if (*pt == OP_Q) {
    pointer_seek(pp, i, lnum);
    
  } else if (*pt == OP_R) {
    pointer_advance(pp, i, lnum);
    
  } else if (*pt == OP_G) {
    pointer_grace(pp, i, core_rstack_current(lnum), lnum);
    
  } else if (*pt == OP_T) {
    pointer_tilt(pp, i, lnum);
    
  } else if (*pt == OP_M) {
    pointer_moment(pp, i, lnum);
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  core_push_p(pp, lnum);
}

static void op_rpush(void *pCustom, long lnum) {
  RULER *pr = NULL;
  
  (void) pCustom;
  
  pr = core_pop_r(lnum);
  core_rstack_push(pr, lnum);
}

static void op_rpop(void *pCustom, long lnum) {
  (void) pCustom;
  
  core_rstack_pop(lnum);
}

static void op_reset(void *pCustom, long lnum) {
  POINTER *pp = NULL;
  
  (void) pCustom;
  
  pp = core_pop_p(lnum);
  pointer_reset(pp);
  core_push_p(pp, lnum);
}

static void op_bpm(void *pCustom, long lnum) {
  
  int32_t num = 0;
  int32_t denom = 0;
  int32_t unit = 0;
  double f = 0.0;
  int32_t i = 0;
  
  (void) pCustom;
  
  unit = core_pop_i(lnum);
  denom = core_pop_i(lnum);
  num = core_pop_i(lnum);
  
  if (num < 1) {
    raiseErr(__LINE__, "bpm numerator must be at least one on line %ld",
      srcLine(lnum));
  }
  if (denom < 1) {
    raiseErr(__LINE__,
      "bpm denominator must be at least one on line %ld",
      srcLine(lnum));
  }
  if (unit < 1) {
    raiseErr(__LINE__, "bpm unit must be at least one on line %ld",
      srcLine(lnum));
  }
  
  f = ((double) num) / ((double) denom);
  f = f * (((double) unit) / 24.0);
  f = 60000000.0 / f;
  
  if (!isfinite(f)) {
    raiseErr(__LINE__,
      "bpm calculation has non-finite result on line %ld",
      srcLine(lnum));
  }
  if (!(f >= (double) MIDI_TEMPO_MIN)) {
    f = 1.0;
  } else if (!(f <= (double) MIDI_TEMPO_MAX)) {
    f = (double) MIDI_TEMPO_MAX;
  }
  
  i = (int32_t) floor(f);
  if (i < MIDI_TEMPO_MIN) {
    i = MIDI_TEMPO_MIN;
  } else if (i > MIDI_TEMPO_MAX) {
    i = MIDI_TEMPO_MAX;
  }
  
  core_push_i(i, lnum);
}

/*
 * Registration function
 * =====================
 */

void op_base_register(void) {
  main_op("pop", &op_pop, NULL);
  main_op("dup", &op_dup, NULL);
  main_op("print", &op_print, NULL);
  main_op("newline", &op_newline, NULL);
  main_op("stop", &op_stop, NULL);
  
  main_op("add", &op_binary, &i_add);
  main_op("sub", &op_binary, &i_sub);
  main_op("mul", &op_binary, &i_mul);
  main_op("div", &op_binary, &i_div);
  
  main_op("neg", &op_neg, NULL);
  
  main_op("s", &op_ptr_math, &i_s);
  main_op("q", &op_ptr_math, &i_q);
  main_op("r", &op_ptr_math, &i_r);
  main_op("g", &op_ptr_math, &i_g);
  main_op("t", &op_ptr_math, &i_t);
  main_op("m", &op_ptr_math, &i_m);
  
  main_op("rpush", &op_rpush, NULL);
  main_op("rpop", &op_rpop, NULL);
  main_op("reset", &op_reset, NULL);
  main_op("bpm", &op_bpm, NULL);
}
