/*
 * test_blob.c
 * ===========
 * 
 * Test program for the blob module of Infrared.
 * 
 * Syntax
 * ------
 * 
 *   test_blob
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Infrared modules:
 * 
 *   - diagnostic.c
 *   - blob.c
 */

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "blob.h"
#include "diagnostic.h"

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

static const char *BASE_STR =
  "00 01 02 03 04 05 06 07 08 09 0a 0B 0c 0D 0e 0F"
  "10 11 12 13 14 15 16 17 18 19 1a 1B 1c 1D 1e 1F"
  "20 21 22 23 24 25 26 27 28 29 2a 2B 2c 2D 2e 2F"
  "30 31 32 33 34 35 36 37 38 39 3a 3B 3c 3D 3e 3F"
  "00 01 02 03 04 05 06 07 08 09 0a 0B 0c 0D 0e 0F"
  "10 11 12 13 14 15 16 17 18 19 1a 1B 1c 1D 1e 1F"
  "20 21 22 23 24 25 26 27 28 29 2a 2B 2c 2D 2e 2F"
  "30 31 32 33 34 35 36 37 38 39 3a 3B 3c 3D 3e 3F"
  "00 01 02 03 04 05 06 07 08 09 0a 0B 0c 0D 0e 0F"
  "10 11 12 13 14 15 16 17 18 19 1a 1B 1c 1D 1e 1F"
  "20 21 22 23 24 25 26 27 28 29 2a 2B 2c 2D 2e 2F"
  "30 31 32 33 34 35 36 37 38 39 3a 3B 3c 3D 3e 3F"
  "00 01 02 03 04 05 06 07 08 09 0a 0B 0c 0D 0e 0F"
  "10 11 12 13 14 15 16 17 18 19 1a 1B 1c 1D 1e 1F"
  "20 21 22 23 24 25 26 27 28 29 2a 2B 2c 2D 2e 2F"
  "30 31 32 33 34 35 36 37 38 39 3a 3B 3c 3D 3e 3F"
  "00000000000000000000000000000000";

/*
 * Program entrypoint
 * ==================
 */

int main(int argc, char *argv[]) {
  
  BLOB *pBase = NULL;
  BLOB *pMulti = NULL;
  BLOB *pSub = NULL;
  BLOB *ppAr[3];
  int32_t i = 0;
  int32_t j = 0;
  const uint8_t *pc = NULL;
  
  memset(ppAr, 0, sizeof(BLOB *) * 3);
  
  diagnostic_startup(argc, argv, "test_blob");
  
  if (argc > 1) {
    raiseErr(__LINE__, "Not expecting program arguments");
  }
  
  pBase = blob_fromHex(BASE_STR, 5);
  
  if (blob_len(pBase) != 272) {
    raiseErr(__LINE__, NULL);
  }
  
  pc = blob_ptr(pBase);
  for(i = 0; i < 272; i++) {
    if (i >= 256) {
      if (pc[i] != 0) {
        raiseErr(__LINE__, NULL);
      }
    } else {
      if ((i % 64) != pc[i]) {
        raiseErr(__LINE__, NULL);
      }
    }
  }
  
  ppAr[0] = pBase;
  ppAr[1] = pBase;
  ppAr[2] = pBase;
  
  pMulti = blob_concat(ppAr, 3, 10);
  
  if (blob_len(pMulti) != 272 * 3) {
    raiseErr(__LINE__, NULL);
  }
  
  pc = blob_ptr(pMulti);
  for(i = 0; i < 272 * 3; i++) {
    j = i % 272;
    if (j >= 256) {
      if (pc[i] != 0) {
        raiseErr(__LINE__, NULL);
      }
    } else {
      if ((j % 64) != pc[i]) {
        raiseErr(__LINE__, NULL);
      }
    }
  }
  
  pSub = blob_slice(pMulti, 16, 32, 15);
  
  if (blob_len(pSub) != 16) {
    raiseErr(__LINE__, NULL);
  }
  
  pc = blob_ptr(pSub);
  for(i = 0; i < 16; i++) {
    if (pc[i] != i + 0x10) {
      raiseErr(__LINE__, NULL);
    }
  }
  
  blob_shutdown();
  
  diagnostic_log("Test successful");
  return EXIT_SUCCESS;
}
