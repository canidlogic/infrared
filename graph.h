#ifndef GRAPH_H_INCLUDED
#define GRAPH_H_INCLUDED

/*
 * graph.h
 * =======
 * 
 * Graph manager of Infrared.
 * 
 * Requirements
 * ------------
 * 
 * May require the <math.h> library with -lm
 * 
 * Requires the following Infrared modules:
 * 
 *   - diagnostic.c
 *   - pointer.c
 */

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include "pointer.h"

/*
 * Type declarations
 * =================
 */

/*
 * GRAPH structure prototype.
 * 
 * See the implementation file for definition.
 */
struct GRAPH_TAG;
typedef struct GRAPH_TAG GRAPH;

/*
 * Function pointer types
 * ======================
 */

/*
 * Callback function pointer type for graph_track().
 * 
 * Parameters:
 * 
 *   pCustom - passed through from graph_track() caller
 * 
 *   t - the moment offset of a change in value
 * 
 *   v - the new value being changed to
 */
typedef void (*graph_fp_track)(
    void    * pCustom,
    int32_t   t,
    int32_t   v);

/*
 * Public functions
 * ================
 */

/*
 * Directly get a new graph object that has the given constant value at
 * all time points.
 * 
 * The accumulator is bypassed by this function, so there may or may not
 * be a graph object definition in progress.  It has no effect on this
 * function.
 * 
 * The graph object that is constructed is no different than the graph
 * object that would have been constructed in the accumulator using a
 * single constant region.  This separate function is necessary for
 * internal use in the render module, which must be able to construct a
 * default graph regardless of the graph accumulator state.
 * 
 * The constant value can be any value zero or greater.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   v - the constant value of the graph
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
GRAPH *graph_constant(int32_t v, long lnum);

/*
 * Begin the definition of a new graph object.
 * 
 * There must not already be a graph object definition in progress or an
 * error occurs.
 * 
 * The new graph starts out empty with no regions defined.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void graph_begin(long lnum);

/*
 * Add a constant region to the open graph definition.
 * 
 * There must be a graph definition in progress or an error occurs.
 * 
 * The region begins at the moment offset that the provided pointer
 * computes to.  (Header pointers may not be used.)  If this is not the
 * first region in the graph, the computed moment offset must be greater
 * than the moment offset of the previous region that was added to the
 * graph.
 * 
 * The region has the constant value v throughout the region.  This can
 * be any value zero or greater.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - pointer to start of the new region
 * 
 *   v - the constant value for the region
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void graph_add_constant(POINTER *pp, int32_t v, long lnum);

/*
 * Add a ramp region to the open graph definition.
 * 
 * There must be a graph definition in progress or an error occurs.
 * 
 * The region begins at the moment offset that the provided pointer
 * computes to.  (Header pointers may not be used.)  If this is not the
 * first region in the graph, the computed moment offset must be greater
 * than the moment offset of the previous region that was added to the
 * graph.
 * 
 * The ramp transitions from the graph value a to the graph value b.
 * Both a and b must be zero or greater.  If a and b are equal, this
 * call will be routed to graph_add_constant() instead.
 * 
 * The s parameter is the step distance in subquanta, which must be
 * greater than zero.  Changes in ramp value occur only at subquanta
 * offsets that are divisible by the step distance.
 * 
 * If use_log is non-zero, then logarithmic interpolation will be used
 * in the ramp.  Otherwise, linear interpolation will be used.
 * 
 * A ramp may not end up as the last region in a graph because it needs
 * the start of a following region to determine the duration of the
 * ramp.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - pointer to start of the new region
 * 
 *   a - the starting graph value of the ramp
 * 
 *   b - the target graph value of the ramp
 * 
 *   s - the step distaince for the ramp
 * 
 *   use_log - non-zero for logarithmic interpolation, zero for linear
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void graph_add_ramp(
    POINTER * pp,
    int32_t   a,
    int32_t   b,
    int32_t   s,
    int       use_log,
    long      lnum);

/*
 * Add a derived region to the open graph definition.
 * 
 * There must be a graph definition in progress or an error occurs.
 * 
 * The region begins at the moment offset that the pDerive pointer
 * computes to.  (Header pointers may not be used.)  If this is not the
 * first region in the graph, the computed moment offset must be greater
 * than the moment offset of the previous region that was added to the
 * graph.
 * 
 * Graph values are copied from the given graph object, starting at the
 * moment offset computed from the pSource pointer.  (Header pointers
 * may not be used.)
 * 
 * Each of the copied graph values is transformed before being added to
 * the open graph definition.  First, the value is multiplied by the
 * numerator and divided by the denominator, with floor rounding down to
 * an integer.  The numerator must be zero or greater and the
 * denominator must be greater than zero.
 * 
 * Second, the value is added to the c value given, which may have any
 * integer value, including negative values.
 * 
 * Third, the value is clamped to be within the range min_val to
 * max_val, inclusive of boundaries.  min_val must be zero or greater
 * and max_val must be greater than or equal to min_val.  If max_val has
 * the special value -1, there is no maximum value limit.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pDerive - pointer to start of the new region
 * 
 *   pg - the source graph to copy values from
 * 
 *   pSource - pointer to start of region to copy in source graph
 * 
 *   numerator - numerator of scaling value
 * 
 *   denominator - denominator of scaling value
 * 
 *   c - value to add after scaling
 * 
 *   min_val - the minimum value for transformed values
 * 
 *   max_val - the maximum value for transformed values, or -1 if no
 *   maximum
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void graph_add_derived(
    POINTER * pDerive,
    GRAPH   * pg,
    POINTER * pSource,
    int32_t   numerator,
    int32_t   denominator,
    int32_t   c,
    int32_t   min_val,
    int32_t   max_val,
    long      lnum);

/*
 * End the definition of a new graph object.
 * 
 * There must be a graph definition in progress or an error occurs.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the completed graph object
 */
GRAPH *graph_end(long lnum);

/*
 * Shut down the graph system, releasing all graphs that have been
 * allocated and preventing all further calls to graph functions, except
 * for graph_shutdown().
 */
void graph_shutdown(void);

/*
 * Determine the value of a given graph at the given moment offset.
 * 
 * Parameters:
 * 
 *   pg - the graph
 * 
 *   t - the moment offset
 * 
 * Return:
 * 
 *   the value of the graph, which is always zero or greater
 */
int32_t graph_query(GRAPH *pg, int32_t t);

/*
 * Use the tracking algorithm to report all changes in graph value to a
 * given callback function.
 * 
 * t_start is the moment offset to start tracking.  This is required.
 * 
 * t_end and v_start are both optional, with their presence indicated by
 * the has_end and has_v_start flag parameters.
 * 
 * If has_end is non-zero, then t_end must be greater than or equal to
 * t_start.  In this case, the tracked time range is closed.  If has_end
 * is zero, t_end is ignored and the tracked time range is open.
 * 
 * If has_v_start is non-zero, then no change in value is reported at
 * t_start if the graph value at t_start is equal to v_start.  If
 * has_v_start is zero, then v_start is ignored and a change of value is
 * always reported at t_start.
 * 
 * Changes in graph value will be reported to the given callback
 * function in chronological order.
 * 
 * Parameters:
 * 
 *   pg - the graph
 * 
 *   fp - the callback function
 * 
 *   pCustom - passed through to the callback function
 * 
 *   t_start - the starting offset for tracking
 * 
 *   t_end - the ending offset for tracking, if has_end is non-zero
 * 
 *   v_start - the starting value, if has_v_start is non-zero
 * 
 *   has_end - non-zero if t_end is provided
 * 
 *   has_v_start - non-zero if v_start is provided
 */
void graph_track(
    GRAPH          * pg,
    graph_fp_track   fp,
    void           * pCustom,
    int32_t          t_start,
    int32_t          t_end,
    int32_t          v_start,
    int              has_end,
    int              has_v_start);

/*
 * Print a textual representation of a graph to the given output file.
 * 
 * This is intended for diagnostics.  The format is a sequence of nodes
 * separated by spaces.  Each node is a moment offset and a graph value,
 * both signed integers, separated by a comma.
 * 
 * No line break is printed after the textual representation.
 * 
 * Parameters:
 * 
 *   pg - the graph
 * 
 *   pOut - the file to print the set to
 */
void graph_print(GRAPH *pg, FILE *pOut);

#endif
