#ifndef OP_RENDER_H_INCLUDED
#define OP_RENDER_H_INCLUDED

/*
 * op_render.h
 * ===========
 * 
 * Rendering operations module.  This module implements the "Rendering
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
 *   - art.c
 *   - core.c
 *   - diagnostic.c
 *   - graph.c
 *   - main.c
 *   - render.c
 *   - ruler.c
 *   - set.c
 */

void op_render_register(void);

#endif
