#ifndef OP_CONTROL_H_INCLUDED
#define OP_CONTROL_H_INCLUDED

/*
 * op_control.h
 * ============
 * 
 * Control operations module.  This module implements the "Control
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
 *   - control.c
 *   - core.c
 *   - diagnostic.c
 *   - graph.c
 *   - main.c
 *   - midi.c
 *   - pointer.c
 *   - text.c
 */

void op_control_register(void);

#endif
