/*
 * test_graph.c
 * ============
 * 
 * Test program for the graph module of Infrared.
 * 
 * Syntax
 * ------
 * 
 *   test_graph
 * 
 * Requirements
 * ------------
 * 
 * May require the <math.h> library with -lm
 * 
 * Requires the following Infrared modules:
 * 
 *   - diagnostic.c
 *   - graph.c
 *   - pointer.c
 *   - ruler.c
 * 
 * Requires the following external libraries:
 * 
 *   - libnmf
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "diagnostic.h"
#include "graph.h"
#include "pointer.h"
#include "ruler.h"

#include "nmf.h"

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
  
  NMF_DATA *pd = NULL;
  GRAPH *pFirst = NULL;
  GRAPH *pSecond = NULL;
  POINTER *pp = NULL;
  POINTER *pp2 = NULL;
  
  diagnostic_startup(argc, argv, "test_graph");
  
  if (argc > 1) {
    raiseErr(__LINE__, "Not expecting program arguments");
  }
  
  pd = nmf_alloc();
  pointer_init(pd);
  pp = pointer_new();
  pp2 = pointer_new();
  
  /* === */
  
  graph_begin(__LINE__);
  
  pointer_reset(pp);
  pointer_jump(pp, 0, __LINE__);
  pointer_seek(pp, 0, __LINE__);
  pointer_moment(pp, -1, __LINE__);
  graph_add_ramp(pp, 0, 127, 5, 1, __LINE__);
  
  pointer_advance(pp, 100, __LINE__);
  graph_add_constant(pp, 127, __LINE__);
  
  pointer_advance(pp, 100, __LINE__);
  graph_add_ramp(pp, 127, 0, 5, 1, __LINE__);
  
  pointer_advance(pp, 100, __LINE__);
  graph_add_constant(pp, 0, __LINE__);
  
  printf("First: ");
  pFirst = graph_end(__LINE__);
  graph_print(pFirst, stdout);
  printf("\n\n");
  
  /* === */
  
  printf("Query first at moment 1210: %ld\n\n",
    (long) graph_query(pFirst, 1210));
  
  /* === */
  
  graph_begin(__LINE__);
  
  pointer_reset(pp);
  pointer_jump(pp, 0, __LINE__);
  
  pointer_reset(pp2);
  pointer_jump(pp2, 0, __LINE__);
  pointer_advance(pp2, 195, __LINE__);
  
  graph_add_derived(pp, pFirst, pp2, 1, 2, 0, 0, -1, __LINE__);
  
  printf("Second: ");
  pSecond = graph_end(__LINE__);
  graph_print(pSecond, stdout);
  printf("\n\n");
  
  /* === */
  
  printf("Query second at moment -5: %ld\n\n",
    (long) graph_query(pSecond, -5));
  
  /* === */
  
  nmf_free(pd);
  pd = NULL;
  
  graph_shutdown();
  pointer_shutdown();
  ruler_shutdown();
  
  diagnostic_log("Test successful");
  return EXIT_SUCCESS;
}
