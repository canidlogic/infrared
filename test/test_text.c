/*
 * test_text.c
 * ===========
 * 
 * Test program for the text module of Infrared.
 * 
 * Syntax
 * ------
 * 
 *   test_text
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Infrared modules:
 * 
 *   - diagnostic.c
 *   - text.c
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostic.h"
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
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  TEXT *pHello = NULL;
  TEXT *pSpace = NULL;
  TEXT *pWorld = NULL;
  TEXT *pMulti = NULL;
  TEXT *pSub = NULL;
  TEXT *ppAr[3];
  
  memset(ppAr, 0, sizeof(TEXT *) * 3);
  
  diagnostic_startup(argc, argv, "test_text");
  
  if (argc > 1) {
    raiseErr(__LINE__, "Not expecting program arguments");
  }
  
  pHello = text_literal("Hello", 5);
  pSpace = text_literal(" ", 6);
  pWorld = text_literal("world!", 7);
  
  ppAr[0] = pHello;
  ppAr[1] = pSpace;
  ppAr[2] = pWorld;
  
  pMulti = text_concat(ppAr, 3, 10);
  
  diagnostic_log("Full message: %s", text_ptr(pMulti));
  
  pSub = text_slice(pMulti, 6, 11, 15);
  
  diagnostic_log("World: %s", text_ptr(pSub));
  
  text_shutdown();
  
  diagnostic_log("Test successful");
  return EXIT_SUCCESS;
}
