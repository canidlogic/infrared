#ifndef RULER_H_INCLUDED
#define RULER_H_INCLUDED

/*
 * ruler.h
 * =======
 * 
 * Ruler manager of Infrared.
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Infrared modules:
 * 
 *   - diagnostic.c
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

/*
 * Type declarations
 * =================
 */

/*
 * RULER structure prototype.
 * 
 * See the implementation file for definition.
 */
struct RULER_TAG;
typedef struct RULER_TAG RULER;

/*
 * Public functions
 * ================
 */

/*
 * Create a new ruler object with the given parameter values.
 * 
 * The slot is the amount of subquanta (1/8 of an NMF quantum) that each
 * unmeasured grace note occupies in the performance.  It must be
 * greater than zero.
 * 
 * The gap is the amount to reduce the slot to get the performance
 * duration of the unmeasured grace note.  It must be a subquanta value
 * that is zero or negative, and furthermore when the slot is reduced by
 * the gap, the result must be greater than zero.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   slot - the time in subquanta occupied by unmeasured grace notes
 * 
 *   gap - the amount to reduce the slot to get the performance duration
 *   of unmeasured grace notes
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
RULER *ruler_new(int32_t slot, int32_t gap, long lnum);

/*
 * Shut down the ruler system, releasing all rulers that have been
 * allocated and preventing all further calls to ruler functions, except
 * for ruler_shutdown().
 */
void ruler_shutdown(void);

/*
 * Compute the starting performance time offset of an unmeasured grace
 * note.
 * 
 * beat is the time offset of the beat that the grace note is attached
 * to, expressed in subquantum (1/8 of an NMF quantum) units.  It may
 * have any value, including negative time offsets.
 * 
 * i is the grace note index, which must be less than zero.  -1 means
 * the grace note immediately before the beat, -2 means the grace note
 * immediately before that, and so forth.
 * 
 * The computed offset is expressed in subquanta, and it might be
 * negative.
 * 
 * Parameters:
 * 
 *   pr - the ruler
 * 
 *   beat - the subquanta offset of the main beat
 * 
 *   i - the grace note index
 * 
 * Return:
 * 
 *   the subquanta offset of the grace note, possibly negative
 */
int32_t ruler_pos(RULER *pr, int32_t beat, int32_t i);

/*
 * Determine the performance duration in subquanta (1/8 of an NMF
 * quantum) of an unmeasured grace note.
 * 
 * Parameters:
 * 
 *   pr - the ruler
 * 
 * Return:
 * 
 *   the performance duration in subquanta, greater than zero
 */
int32_t ruler_dur(RULER *pr);

/*
 * Print a textual representation of a ruler to the given output file.
 * 
 * This is intended for diagnostics.  The format is a pair of signed
 * integers.  The first signed integer is the slot, and the second
 * signed integer is the gap.
 * 
 * No line break is printed after the textual representation.
 * 
 * Parameters:
 * 
 *   pr - the ruler
 * 
 *   pOut - the file to print to
 */
void ruler_print(RULER *pr, FILE *pOut);

#endif
