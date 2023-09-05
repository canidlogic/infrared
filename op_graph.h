#ifndef OP_GRAPH_H_INCLUDED
#define OP_GRAPH_H_INCLUDED

/*
 * op_graph.h
 * ==========
 * 
 * Graph construction module.  This module implements the "Graph
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
 *   - graph.c
 *   - main.c
 *   - pointer.c
 */

void op_graph_register(void);

#endif
