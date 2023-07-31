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
 * Constants
 * =========
 */

/*
 * MIDI velocity constants.
 * 
 * The maximum MIDI velocity is always ART_VEL_MAX.  The minimum MIDI
 * velocity is either 0 or 1, depending on whether 0 is reserved for a
 * special note-off condition.  For articulations, the minimum is always
 * the value of 1.
 * 
 * The default or "neutral" MIDI velocity is ART_VEL_NEUTRAL.
 * 
 * The MIDI specification recommends that velocities are performed on a
 * logarithmic scale.
 */
#define ART_VEL_NEUTRAL ( 64)
#define ART_VEL_MAX     (127)

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
 * vel is the MIDI note-on velocity to assign to all notes using this
 * articulation.  It must be in range 1 to ART_VEL_MAX, inclusive.  The
 * value ART_VEL_NEUTRAL is considered neutral, neither loud nor soft.
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
 * The scaling factor, bumper, and gap are only applied to measured
 * (non-grace) notes.  All articulations must have valid values for
 * these fields, but unmeasured grace notes will only make use of the
 * velocity field.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   vel - the MIDI velocity
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
    int32_t vel,
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
 * Get the MIDI note-on velocity of the given articulation.
 * 
 * Parameters:
 * 
 *   pa - the articulation
 * 
 * Return:
 * 
 *   the MIDI velocity, in range 1 to ART_VEL_MAX, inclusive
 */
int art_velocity(ART *pa);

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
