/*
 * test_render.c
 * =============
 * 
 * Test program for the note rendering module of Infrared.
 * 
 * Syntax
 * ------
 * 
 *   test_render < input.nmf > test.mid
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

#include "diagnostic.h"
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
  
  diagnostic_startup(argc, argv, "test_render");
  
  if (argc > 1) {
    raiseErr(__LINE__, "Not expecting program arguments");
  }
  
  pd = nmf_parse(stdin);
  pointer_init(pd);
  
  /* === */

  midi_null(-1 * 96 * 8 * 3, 0);
  midi_null(10 * 96 * 8 * 3, 0);
  
  midi_tempo(0, 1, INT32_C(1000000));
  midi_message(0, 1, 1, MIDI_MSG_PROGRAM, 0, 0);
  
  render_nmf(pd);
  
  midi_compile(stdout);
  
  /* === */
  
  nmf_free(pd);
  pd = NULL;
  
  pointer_shutdown();
  
  diagnostic_log("Test successful");
  return EXIT_SUCCESS;
}
