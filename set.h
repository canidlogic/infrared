#ifndef SET_H_INCLUDED
#define SET_H_INCLUDED

/*
 * set.h
 * =====
 * 
 * Set manager of Infrared.
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Infrared modules:
 * 
 *   - diagnostic.c
 */

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

/*
 * Type declarations
 * =================
 */

/*
 * SET structure prototype.
 * 
 * See the implementation file for definition.
 */
struct SET_TAG;
typedef struct SET_TAG SET;

/*
 * Public functions
 * ================
 */

/*
 * Begin the definition of a new set object.
 * 
 * There must not already be a set object definition in progress or an
 * error occurs.
 * 
 * The new set starts out as an empty set, which is a type of "positive"
 * set.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void set_begin(long lnum);

/*
 * Include all possible integers in the open set definition.
 * 
 * There must be a set definition in progress or an error occurs.
 * 
 * The current set definition will be replaced with a "negative" set
 * that includes every possible integer value.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void set_all(long lnum);

/*
 * Clear the open set definition back to an empty set.
 * 
 * There must be a set definition in progress or an error occurs.
 * 
 * The current set definition will be replaced with a "positive" set
 * that doesn't include any integer values.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void set_none(long lnum);

/*
 * Invert the open set definition.
 * 
 * There must be a set definition in progress or an error occurs.
 * 
 * Each possible integer that is not part of the current set definition
 * will be in the updated definition, while each integer that is in the
 * current set definition will not be in the updated definition.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void set_invert(long lnum);

/*
 * Include or exclude a closed range of integers in the open set
 * definition.
 * 
 * There must be a set definition in progress or an error occurs.
 * 
 * lo and hi are the inclusive boundaries of the closed range.  lo must
 * be zero or greater, and hi must be greater than or equal to lo.
 * 
 * exc is non-zero to exclude this closed range, zero to include this
 * closed range.
 * 
 * The current set definition will be updated by either taking the
 * union (inclusion) or difference (exclusion) with the provided range
 * of integers.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   lo - the lower boundary of the range
 * 
 *   hi - the upper boundary of the range
 * 
 *   exc - non-zero for exclusion, zero for inclusion
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void set_rclose(int32_t lo, int32_t hi, int exc, long lnum);

/*
 * Include or exclude an open range of integers in the open set 
 * definition.
 * 
 * There must be a set definition in progress or an error occurs.
 * 
 * lo is the inclusive lower boundary of the open range.  There is no
 * upper boundary to the open range.  lo must be zero or greater.
 * 
 * exc is non-zero to exclude this open range, zero to include this open
 * range.
 * 
 * The current set definition will be updated by either taking the
 * union (inclusion) or difference (exclusion) with the provided range
 * of integers.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   lo - the lower boundary of the range
 * 
 *   exc - non-zero for exclusion, zero for inclusion
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void set_ropen(int32_t lo, int exc, long lnum);

/*
 * Update the open set definition by taking the union with a provided
 * set object.
 * 
 * There must be a set definition in progress or an error occurs.
 * 
 * The provided set object is not modified.  The open set definition is
 * updated to the union of its current state and the provided set.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   ps - the set object to operate with
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void set_union(SET *ps, long lnum);

/*
 * Update the open set definition by taking the intersection with a
 * provided set object.
 * 
 * There must be a set definition in progress or an error occurs.
 * 
 * The provided set object is not modified.  The open set definition is
 * updated to the intersection of its current state and the provided
 * set.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   ps - the set object to operate with
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void set_intersect(SET *ps, long lnum);

/*
 * Update the open set definition by excluding all values that are in a
 * provided set object.
 * 
 * There must be a set definition in progress or an error occurs.
 * 
 * The provided set object is not modified.  The open set definition is
 * updated to the intersection of its current state and the inverse of
 * the provided set.  This is the set difference operation.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   ps - the set object to operate with
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void set_except(SET *ps, long lnum);

/*
 * End the definition of a new set object.
 * 
 * There must be a set definition in progress or an error occurs.
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
 *   the completed set object
 */
SET *set_end(long lnum);

/*
 * Shut down the set system, releasing all sets that have been allocated
 * and preventing all further calls to set functions, except for
 * set_shutdown().
 */
void set_shutdown(void);

/*
 * Check whether a given value is in a set.
 * 
 * val is the value to check, which must be zero or greater.
 * 
 * Parameters:
 * 
 *   ps - the set
 * 
 *   val - the value to check for
 * 
 * Return:
 * 
 *   non-zero if value is in set, zero if not
 */
int set_has(SET *ps, int32_t val);

/*
 * Print a textual representation of a set to the given output file.
 * 
 * This is intended for diagnostics.  The format is a sequence of value
 * ranges separated by commas, or "<empty>" if the set is empty.  Each
 * value range is either a single unsigned decimal integer or two
 * unsigned decimal integers with a hyphen between them from lower to
 * upper bounds, inclusive.  An open range at the end is indicated by an
 * unsigned decimal integer followed by a hyphen.
 * 
 * No line break is printed after the textual representation.
 * 
 * Parameters:
 * 
 *   ps - the set
 * 
 *   pOut - the file to print the set to
 */
void set_print(SET *ps, FILE *pOut);

#endif
