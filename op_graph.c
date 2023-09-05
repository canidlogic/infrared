/*
 * op_graph.c
 * ==========
 * 
 * Implementation of op_graph.h
 * 
 * See the header for further information.  See the Infrared operations
 * documentation for specifications of the operations.
 */

#include "op_graph.h"

#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "diagnostic.h"
#include "graph.h"
#include "pointer.h"
#include "main.h"

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

static int m_true = 1;
static int m_false = 0;

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

static void op_gval(void *pCustom, long lnum) {
  int32_t v = 0;
  
  (void) pCustom;
  
  v = core_pop_i(lnum);
  core_push_g(graph_constant(v, lnum), lnum);
}

static void op_begin_graph(void *pCustom, long lnum) {
  (void) pCustom;
  
  graph_begin(lnum);
}

static void op_end_graph(void *pCustom, long lnum) {
  (void) pCustom;
  
  core_push_g(graph_end(lnum), lnum);
}

static void op_graph_const(void *pCustom, long lnum) {
  POINTER *pp = NULL;
  int32_t v = 0;
  
  (void) pCustom;
  
  v = core_pop_i(lnum);
  pp = core_pop_p(lnum);
  
  graph_add_constant(pp, v, lnum);
}

/*
 * pCustom points to an int that is non-zero for logarithmic or zero for
 * linear.
 */
static void op_graph_ramp(void *pCustom, long lnum) {
  POINTER *pp = NULL;
  int32_t a = 0;
  int32_t b = 0;
  int32_t s = 0;
  int *pt = NULL;
  
  if (pCustom == NULL) {
    raiseErr(__LINE__, NULL);
  }
  pt = (int *) pCustom;
  
  s = core_pop_i(lnum);
  b = core_pop_i(lnum);
  a = core_pop_i(lnum);
  pp = core_pop_p(lnum);
  
  graph_add_ramp(pp, a, b, s, *pt, lnum);
}

static void op_graph_derive(void *pCustom, long lnum) {
  POINTER *pp = NULL;
  GRAPH *pg = NULL;
  POINTER *pSrc = NULL;
  int32_t num = 0;
  int32_t denom = 0;
  int32_t c = 0;
  int32_t min = 0;
  int32_t max = 0;
  
  (void) pCustom;
  
  max = core_pop_i(lnum);
  min = core_pop_i(lnum);
  c = core_pop_i(lnum);
  denom = core_pop_i(lnum);
  num = core_pop_i(lnum);
  pSrc = core_pop_p(lnum);
  pg = core_pop_g(lnum);
  pp = core_pop_p(lnum);
  
  graph_add_derived(pp, pg, pSrc, num, denom, c, min, max, lnum);
}

/*
 * Registration function
 * =====================
 */

void op_graph_register(void) {
  main_op("gval", &op_gval, NULL);
  main_op("begin_graph", &op_begin_graph, NULL);
  main_op("end_graph", &op_end_graph, NULL);
  main_op("graph_const", &op_graph_const, NULL);
  
  main_op("graph_ramp", &op_graph_ramp, &m_false);
  main_op("graph_ramp_log", &op_graph_ramp, &m_true);
  
  main_op("graph_derive", &op_graph_derive, NULL);
}
