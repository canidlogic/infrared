/*
 * test_control.c
 * ==============
 * 
 * Test program for the control module of Infrared.
 * 
 * Syntax
 * ------
 * 
 *   test_control < input.nmf > test.mid
 * 
 * Requirements
 * ------------
 * 
 * May require the <math.h> library with -lm
 * 
 * Requires the following Infrared modules:
 * 
 *   - art.c
 *   - blob.c
 *   - control.c
 *   - diagnostic.c
 *   - graph.h
 *   - midi.c
 *   - pointer.c
 *   - render.c
 *   - ruler.c
 *   - set.c
 *   - text.c
 * 
 * Requires the following external libraries:
 * 
 *   - libnmf
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "control.h"
#include "diagnostic.h"
#include "graph.h"
#include "pointer.h"
#include "render.h"

#include "nmf.h"

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
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  NMF_DATA *pd = NULL;
  POINTER *pp = NULL;
  GRAPH *pg = NULL;
  
  diagnostic_startup(argc, argv, "test_control");
  
  if (argc > 1) {
    raiseErr(__LINE__, "Not expecting program arguments");
  }
  
  pd = nmf_parse(stdin);
  pointer_init(pd);
  
  /* === */

  pp = pointer_new();
  
  pointer_jump(pp, 0, __LINE__);
  pointer_seek(pp, -1, __LINE__);
  control_null(pp, __LINE__);
  
  pointer_seek(pp, 10, __LINE__);
  control_null(pp, __LINE__);
  
  pointer_seek(pp, 0, __LINE__);
  control_instrument(pp, 1, 0, 1, 0, __LINE__);
  
  graph_begin(__LINE__);
  
  pointer_reset(pp);
  pointer_jump(pp, 0, __LINE__);
  pointer_moment(pp, -1, __LINE__);
  graph_add_ramp(pp, 1000000, 250000, 96 * 8, 1, __LINE__);
  
  pointer_reset(pp);
  pointer_jump(pp, 0, __LINE__);
  pointer_seek(pp, 96 * 8, __LINE__);
  pointer_moment(pp, -1, __LINE__);
  graph_add_constant(pp, 250000, __LINE__);
  
  pg = graph_end(__LINE__);
  
  control_auto(CONTROL_TYPE_TEMPO, 0, 0, pg, __LINE__);
  
  render_nmf(pd);
  control_track();
  midi_compile(stdout);
  
  /* === */
  
  nmf_free(pd);
  pd = NULL;
  
  pointer_shutdown();
  graph_shutdown();
  
  diagnostic_log("Test successful");
  return EXIT_SUCCESS;
}
