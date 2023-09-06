/*
 * test_midi.c
 * ===========
 * 
 * Test program for the MIDI module of Infrared.
 * 
 * Syntax
 * ------
 * 
 *   test_midi > test.mid
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Infrared modules:
 * 
 *   - blob.c
 *   - diagnostic.c
 *   - midi.c
 *   - pointer.c
 *   - ruler.c
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
#include "midi.h"
#include "pointer.h"

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
  
  diagnostic_startup(argc, argv, "test_midi");
  
  if (argc > 1) {
    raiseErr(__LINE__, "Not expecting program arguments");
  }
  
  pd = nmf_alloc();
  pointer_init(pd);
  
  /* === */

  midi_null(-1 * 96 * 8 * 3, 0);
  midi_null(10 * 96 * 8 * 3, 0);
  
  midi_tempo(0, 1, INT32_C(1000000));
  midi_time_sig(0, 1, 4, 4, 24);
  midi_key_sig(0, 1, 0, 0);
  midi_message(0, 1, 1, MIDI_MSG_PROGRAM, 0, 0);
  
  midi_message(0 * 96 * 8 * 3, 0, 1, MIDI_MSG_NOTE_ON, 60, 64);
  midi_message(1 * 96 * 8 * 3, 0, 1, MIDI_MSG_NOTE_ON, 60, 0);
  
  midi_message(2 * 96 * 8 * 3, 0, 1, MIDI_MSG_NOTE_ON, 62, 64);
  midi_message(3 * 96 * 8 * 3, 0, 1, MIDI_MSG_NOTE_ON, 62, 0);
  
  midi_message(4 * 96 * 8 * 3, 0, 1, MIDI_MSG_NOTE_ON, 64, 64);
  midi_message(5 * 96 * 8 * 3, 0, 1, MIDI_MSG_NOTE_ON, 64, 0);
  
  midi_compile(stdout);
  
  /* === */
  
  nmf_free(pd);
  pd = NULL;
  
  pointer_shutdown();
  
  diagnostic_log("Test successful");
  return EXIT_SUCCESS;
}
