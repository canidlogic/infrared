#ifndef POINTER_H_INCLUDED
#define POINTER_H_INCLUDED

/*
 * pointer.h
 * =========
 * 
 * Pointer manager of Infrared.
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Infrared modules:
 * 
 *   - diagnostic.c
 *   - ruler.c
 * 
 * Requires the following external libraries:
 * 
 *   - libnmf
 */

#include <stddef.h>
#include <stdint.h>

#include "nmf.h"

#include "ruler.h"

/*
 * Type declarations
 * =================
 */

/*
 * POINTER structure prototype.
 * 
 * See the implementation file for definition.
 */
struct POINTER_TAG;
typedef struct POINTER_TAG POINTER;

/*
 * Public functions
 * ================
 */

/*
 * Create a new pointer object.
 * 
 * Pointers always start out as header pointers.
 * 
 * Return:
 * 
 *   the new pointer object
 */
POINTER *pointer_new(void);

/*
 * Shut down the pointer system, releasing all pointers that have been
 * allocated and preventing all further calls to pointer functions,
 * except for pointer_shutdown().
 */
void pointer_shutdown(void);

/*
 * Reset a pointer back to its initial state as a header pointer.
 * 
 * Parameters:
 * 
 *   pp - the pointer
 */
void pointer_reset(POINTER *pp);

/*
 * Move a pointer to the start of a given NMF section.
 * 
 * The quantum offset is reset to zero, any grace pickup is cleared, and
 * any tilt is zeroed out.  If the pointer is currently a header
 * pointer, its moment part will be initialized to middle-of-moment.
 * Otherwise, the moment part will retain its current value.
 * 
 * The section index must be zero or greater.  This function does NOT
 * check that the section is defined in the NMF file; that will be done
 * later when converting the pointer to an absolute offset.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - the pointer
 * 
 *   sect - the section index
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void pointer_jump(POINTER *pp, int32_t sect, long lnum);

/*
 * Move a pointer to a specific quantum offset from the start of the
 * current section.
 * 
 * The pointer must have a section loaded with the jump function.  An
 * error occurs if this function is used on a header pointer.
 * 
 * To move the offset relative to its current position, use
 * pointer_advance() instead of this function.
 * 
 * This function clears any grace pickup and zeroes out any tilt.  The
 * moment part is retained.
 * 
 * Negative offsets are allowed.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - the pointer
 * 
 *   offs - the offset from the start of the section
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void pointer_seek(POINTER *pp, int32_t offs, long lnum);

/*
 * Move a pointer offset relative to its current position.
 * 
 * This is a wrapper around pointer_seek() that takes the current
 * offset, adds rel to it, and then seeks to that offset.  An error
 * occurs if the pointer is a head pointer, or if adding rel to the
 * current quantum offset causes an integer overflow.
 * 
 * Negative values of rel are allowed.
 * 
 * Parameters:
 * 
 *   pp - the pointer
 * 
 *   rel - the relative value to add to the quantum offset
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void pointer_advance(POINTER *pp, int32_t rel, long lnum);

/*
 * Set the grace note offset of a pointer.
 * 
 * The pointer must have a section loaded with the jump function.  An
 * error occurs if this function is used on a header pointer.
 * 
 * The g value is the grace note offset, which is either zero to
 * indicate no grace note offset, or less than zero selecting a
 * particular unmeasured grace note before the beat.
 * 
 * If the g value is non-zero, a ruler must be provided.  If the g value
 * is zero, the ruler parameter is ignored and may be either NULL or
 * non-NULL.
 * 
 * This function zeroes out any tilt.  The moment part is retained.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - the pointer
 * 
 *   p - the negative grace note offset, or zero for no grace note
 * 
 *   pr - the ruler to use for negative grace offsets, or NULL
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void pointer_grace(POINTER *pp, int32_t g, RULER *pr, long lnum);

/*
 * Set the tilt of a pointer.
 * 
 * The tilt is a subquantum (1/8 of an NMF quantum) offset that is added
 * as the final step of converting a pointer into an absolute subquantum
 * offset.  The tilt may be negative.
 * 
 * The pointer must have a section loaded with the jump function.  An
 * error occurs if this function is used on a header pointer.
 * 
 * The moment part of the pointer is retained.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - the pointer
 * 
 *   tilt - the tilt value to set
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void pointer_tilt(POINTER *pp, int32_t tilt, long lnum);

/*
 * Set the moment part of a pointer.
 * 
 * The pointer must have a section loaded with the jump function.  An
 * error occurs if this function is used on a header pointer.
 * 
 * The m value is -1 for start-of-moment, 0 for middle-of-moment, or 1
 * for end-of-moment.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - the pointer
 * 
 *   m - the moment part
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void pointer_moment(POINTER *pp, int32_t m, long lnum);

/*
 * Determine whether a pointer is a header pointer.
 * 
 * Parameters:
 * 
 *   pp - the pointer
 * 
 * Return:
 * 
 *   non-zero if a header pointer, zero if not
 */
int pointer_isHeader(POINTER *pp);

/*
 * Compute the absolute moment offset of a non-header pointer.
 * 
 * The given pointer must not be a header pointer or an error occurs.
 * Use pointer_isHeader() to check.
 * 
 * You must provide the parsed NMF input file so that the pointer
 * section can be decoded.
 * 
 * To compute the absolute moment offset, this function first determines
 * the absolute subquantum offset, where t=0 is the beginning of the
 * performance.  The absolute subquantum offset might be negative.  Each
 * subquantum is 1/8 of an NMF quantum.
 * 
 * After determining the absolute subquantum offset, this result is
 * multiplied by three.  Finally, the value 0 is added to this for
 * start-of-moment position, 1 for middle-of-moment, or 2 for
 * end-of-moment.
 * 
 * Hence, the time scale looks like this:
 * 
 *                    ...
 *   -3 | Subquantum -1, start-of-moment
 *   -2 | Subquantum -1, middle-of-moment
 *   -1 | Subquantum -1, end-of-moment
 *    0 | Subquantum  0, start-of-moment
 *    1 | Subquantum  0, middle-of-moment
 *    2 | Subquantum  0, end-of-moment
 *    3 | Subquantum  1, start-of-moment
 *                    ...
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - the pointer
 * 
 *   pd - the parsed NMF data, used for sections
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the absolute moment offset
 */
int32_t pointer_compute(POINTER *pp, NMF_DATA *pd, long lnum);

/*
 * Print a textual representation of a pointer to the given output file.
 * 
 * This is intended for diagnostics.  Header pointers report "<header>"
 * as their string.  All other pointers report a quintuple.  The first
 * coordinate is the section.   The second is the offset.  The third is
 * either "." if no grace note or a signed integer grace offset, a
 * colon, and a printed ruler representation.  The fourth is the tilt.
 * The fifth is either "start", "mid", or "end".
 * 
 * No line break is printed after the textual representation.
 * 
 * Parameters:
 * 
 *   pp - the pointer
 * 
 *   pOut - the file to print to
 */
void pointer_print(POINTER *pp, FILE *pOut);

#endif
