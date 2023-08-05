#ifndef ART_H_INCLUDED
#define ART_H_INCLUDED

/*
 * art.h
 * =====
 * 
 * Articulation manager of Infrared.
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

/*
 * Type declarations
 * =================
 */

/*
 * ART structure prototype.
 * 
 * See the implementation file for definition.
 */
struct ART_TAG;
typedef struct ART_TAG ART;

/*
 * Public functions
 * ================
 */

/*
 * Create a new articulation object with the given parameter values.
 * 
 * scale_num and scale_denom are the numerator and denominator of the
 * scaling factor to multiply to notated durations to begin the
 * transformation into performance durations.  This scaling operation is
 * performed at subquantum scale, where there are 8 subquanta in a
 * single NMF quantum.
 * 
 * The scaling denominator must be 1, 2, 4, or 8.  The numerator must be
 * at least one and less than or equal to the denominator.  Equivalent
 * rational numbers have equivalent results, such that 1/2, 2/4, and 4/8
 * are interpreted exactly the same.
 * 
 * bumper is the minimum duration to impose after applying the scaling
 * factor to the duration.  bumper is an integer value zero or greater
 * that is measured in subquanta.  If the result of multiplying by the
 * scaling factor is less than bumper, the result is lengthened to the
 * bumper value.
 * 
 * gap is the maximum duration relative to the notated duration to
 * impose after applying the bumper.  gap is an integer value less than
 * or equal to zero that is measured in subquanta.  If the result after
 * applying the bumper is greater than the notated duration reduced by
 * the gap, then the duration is shortened to the notated duration
 * reduced by the gap.  Finally, the performance duration is lengthened
 * to a value of one if it is less than one.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   scale_num - the numerator of the scaling factor
 * 
 *   scale_denom - the denominator of the scaling factor
 * 
 *   bumper - the minimum duration after scaling
 * 
 *   gap - the amount to reduce the notated duration to get the maximum
 *   duration after the bumper is applied
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
ART *art_new(
    int32_t scale_num,
    int32_t scale_denom,
    int32_t bumper,
    int32_t gap,
    long    lnum);

/*
 * Shut down the articulation system, releasing all articulations that
 * have been allocated and preventing all further calls to articulation
 * functions, except for art_shutdown().
 */
void art_shutdown(void);

/*
 * Transform a measured NMF duration in quanta into a performance
 * duration in subquanta according to the articulation object.
 * 
 * Parameters:
 * 
 *   pa - the articulation
 * 
 *   dur - the input NMF duration in quanta, greater than zero
 * 
 * Return:
 * 
 *   the performance duration in subquanta, greater than zero
 */
int32_t art_transform(ART *pa, int32_t dur);

#endif
