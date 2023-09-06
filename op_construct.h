#ifndef OP_CONSTRUCT_H_INCLUDED
#define OP_CONSTRUCT_H_INCLUDED

/*
 * op_construct.h
 * ==============
 * 
 * Simple constructor operations module.  This module implements the
 * "Simple Constructor Operations" section of the Infrared operations
 * specification.
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
 *   - art.c
 *   - core.c
 *   - diagnostic.c
 *   - main.c
 *   - pointer.c
 *   - ruler.c
 */

void op_construct_register(void);

#endif
