/*
 * test_set.c
 * ==========
 * 
 * Test program for the set module of Infrared.
 * 
 * Syntax
 * ------
 * 
 *   test_set
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Infrared modules:
 * 
 *   - diagnostic.c
 *   - set.c
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "diagnostic.h"
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
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  SET *pEmpty = NULL;
  SET *pAll = NULL;
  SET *pNotSeven = NULL;
  SET *pMIDI = NULL;
  SET *pMIDINo7 = NULL;
  SET *pMIDI7 = NULL;
  SET *pRejoin = NULL;
  
  diagnostic_startup(argc, argv, "test_set");
  
  if (argc > 1) {
    raiseErr(__LINE__, "Not expecting program arguments");
  }
  
  /* === */
  
  set_begin(__LINE__);
  set_none(__LINE__);
  pEmpty = set_end(__LINE__);
  
  printf("Empty set: ");
  set_print(pEmpty, stdout);
  printf("\n");
  
  /* === */
  
  set_begin(__LINE__);
  set_all(__LINE__);
  pAll = set_end(__LINE__);
  
  printf("Everything set: ");
  set_print(pAll, stdout);
  printf("\n");
  
  /* === */
  
  set_begin(__LINE__);
  set_all(__LINE__);
  set_rclose(7, 7, 1, __LINE__);
  pNotSeven = set_end(__LINE__);
  
  printf("All but 7: ");
  set_print(pNotSeven, stdout);
  printf("\n");
  
  /* === */
  
  set_begin(__LINE__);
  set_none(__LINE__);
  set_rclose(1, 16, 0, __LINE__);
  pMIDI = set_end(__LINE__);
  
  printf("MIDI channels: ");
  set_print(pMIDI, stdout);
  printf("\n");
  
  /* === */
  
  set_begin(__LINE__);
  set_all(__LINE__);
  set_intersect(pMIDI, __LINE__);
  set_intersect(pNotSeven, __LINE__);
  pMIDINo7 = set_end(__LINE__);
  
  printf("MIDI channels but 7: ");
  set_print(pMIDINo7, stdout);
  printf("\n");
  
  /* === */
  
  set_begin(__LINE__);
  set_all(__LINE__);
  set_intersect(pMIDI, __LINE__);
  set_except(pNotSeven, __LINE__);
  pMIDI7 = set_end(__LINE__);
  
  printf("MIDI channel 7: ");
  set_print(pMIDI7, stdout);
  printf("\n");
  
  /* === */
  
  set_begin(__LINE__);
  set_none(__LINE__);
  set_union(pMIDINo7, __LINE__);
  set_union(pMIDI7, __LINE__);
  pRejoin = set_end(__LINE__);
  
  printf("MIDI rejoined: ");
  set_print(pRejoin, stdout);
  printf("\n");
  
  /* === */
  
  printf("\n");
  printf("Is 3 in MIDI-but-7: %d\n", set_has(pMIDINo7, 3));
  printf("Is 7 in MIDI-but-7: %d\n", set_has(pMIDINo7, 7));
  printf("Is 16 in MIDI-but-7: %d\n", set_has(pMIDINo7, 16));
  printf("Is 25 in MIDI-but-7: %d\n", set_has(pMIDINo7, 25));
  
  /* === */
  
  set_shutdown();
  
  printf("\n");
  diagnostic_log("Test successful");
  return EXIT_SUCCESS;
}
