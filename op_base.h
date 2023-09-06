#ifndef OP_BASE_H_INCLUDED
#define OP_BASE_H_INCLUDED

/*
 * op_base.h
 * =========
 * 
 * Basic operations module.  This module implements the following
 * sections of the Infrared operations specification:
 * 
 *   - Basic operations
 *   - Diagnostic operations
 *   - Arithmetic operations
 * 
 * To install this operations module, import this header in main.c and
 * add a call to the registration function defined by this header to the
 * registerModules() function in main.c
 * 
 * Requirements
 * ------------
 * 
 * May require the <math.h> library with -lm
 * 
 * Requires the following Infrared modules:
 * 
 *   - core.c
 *   - diagnostic.c
 *   - main.c
 *   - midi.c
 *   - pointer.c
 *   - primitive.c
 *   - ruler.c
 */

void op_base_register(void);

#endif
