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
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "diagnostic.h"
#include "main.h"
#include "primitive.h"

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

/*
 * Local data
 * ==========
 */

static int i_add = OP_ADD;
static int i_sub = OP_SUB;
static int i_mul = OP_MUL;
static int i_div = OP_DIV;

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
}
