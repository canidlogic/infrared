#ifndef OP_STRING_H_INCLUDED
#define OP_STRING_H_INCLUDED

/*
 * op_string.h
 * ===========
 * 
 * String operations module.  This module implements the "String
 * operations" section of the Infrared operations specification.
 * 
 * To install this operations module, import this header in main.c and
 * add a call to the registration function defined by this header to the
 * registerModules() function in main.c
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Infrared modules:
 * 
 *   - blob.c
 *   - core.c
 *   - diagnostic.c
 *   - main.c
 *   - text.c
 */

void op_string_register(void);

#endif
