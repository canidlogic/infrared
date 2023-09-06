#ifndef OP_SET_H_INCLUDED
#define OP_SET_H_INCLUDED

/*
 * op_set.h
 * ========
 * 
 * Set construction operations module.  This module implements the "Set
 * construction operations" section of the Infrared operations
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
 *   - core.c
 *   - diagnostic.c
 *   - main.c
 *   - set.c
 */

void op_set_register(void);

#endif
