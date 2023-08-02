#ifndef PRIMITIVE_H_INCLUDED
#define PRIMITIVE_H_INCLUDED

/*
 * primitive.h
 * ===========
 * 
 * Primitive data type utilities for Infrared.
 * 
 * Requirements
 * ------------
 * 
 * May require the <math.h> library with -lm depending on platform.
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
 * The full range of primitive integer values.
 * 
 * This should be the same as the range for the int32_t type, except it
 * excludes the value -2147483648.
 */
#define PRIMITIVE_INT_MIN INT32_C(-2147483647)
#define PRIMITIVE_INT_MAX INT32_C( 2147483647)

/*
 * Primitive range values selecting everything and nothing.
 */
#define PRIMITIVE_RANGE_ALL  UINT64_C(0xffffffffffffffff)
#define PRIMITIVE_RANGE_NONE UINT64_C(0x0000000000000000)

/*
 * Primitive bitmap values selecting everything and nothing.
 */
#define PRIMITIVE_BITMAP_ALL  UINT16_C(0xffff)
#define PRIMITIVE_BITMAP_NONE UINT16_C(0x0000)

/*
 * Type declarations
 * =================
 */

/*
 * Primitive integers just use the int32_t type.
 * 
 * However, note the PRIMITIVE_INT_MIN and PRIMITIVE_INT_MAX range,
 * which is not the same as for int32_t.
 */

/*
 * The range primitive is a bit string of 64 bits.
 * 
 * The least significant bit of the integer is the first bit of the bit
 * string, and the most significant bit of the integer is the last bit
 * of the bit string.
 * 
 * The first 62 bits are flags selecting a set from the 62 NMF
 * articulations.  The 63rd bit is a flag selecting the primary
 * (measured, non-grace) matrix.  The 64th bit is a flag selecting the
 * secondary (unmeasured grace) matrix.  Bits that are set select the
 * corresponding element.
 */
typedef uint64_t primitive_range;

/*
 * The bitmap primitive is a bit string of 16 bits.
 * 
 * The least significant bit of the integer is the first bit of the bit
 * string, and the most significant bit of the integer is the last bit
 * of the bit string.
 * 
 * The first bit (the least significant bit) corresponds to the first
 * MIDI channel, and the last bit (the most significant bit) corresponds
 * to the last MIDI channel.  Bits that are set select the corresponding
 * MIDI channel.
 */
typedef uint16_t primitive_bitmap;

/*
 * Public functions
 * ================
 */

/*
 * Add two integers together, raising error if result is out of range.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   a - the first operand
 * 
 *   b - the second operand
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the result
 */
int32_t primitive_add(int32_t a, int32_t b, long lnum);

/*
 * Subtract b from a, raising error if result is out of range.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   a - the first operand
 * 
 *   b - the second operand
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the result
 */
int32_t primitive_sub(int32_t a, int32_t b, long lnum);

/*
 * Multiply two integers together, raising error if result is out of
 * range.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   a - the first operand
 * 
 *   b - the second operand
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the result
 */
int32_t primitive_mul(int32_t a, int32_t b, long lnum);

/*
 * Divide a by b, raising error if result is out of range or if b is
 * zero.
 * 
 * The division is performed in floating point, and then floor() is used
 * to round down to an integer result.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   a - the first operand
 * 
 *   b - the second operand
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the result
 */
int32_t primitive_div(int32_t a, int32_t b, long lnum);

/*
 * Invert the sign of the given integer.
 * 
 * Positive values are changed to their corresponding negative values.
 * Negative values are changed to their corresponding positive values.
 * Zero is unchanged.
 * 
 * Since the absolute value of PRIMITIVE_INT_MIN is PRIMITIVE_INT_MAX,
 * all valid integers can be successfully inverted.
 * 
 * Parameters:
 * 
 *   a - the operand
 * 
 * Return:
 * 
 *   the result
 */
int32_t primitive_neg(int32_t a);

/*
 * Take the union of two ranges.
 * 
 * Matrices are selected if they are present in either or both of the
 * operands.  Articulations are selected if they are present in either
 * or both of the operands.
 * 
 * Parameters:
 * 
 *   a - the first operand
 * 
 *   b - the second operand
 * 
 * Return:
 * 
 *   the result
 */
primitive_range primitive_range_union(
    primitive_range a,
    primitive_range b);

/*
 * Take the intersection of two ranges.
 * 
 * Matrices are selected if they are present in both of the operands.
 * Articulations are selected if they are present in both of the
 * operands.
 * 
 * Parameters:
 * 
 *   a - the first operand
 * 
 *   b - the second operand
 * 
 * Return:
 * 
 *   the result
 */
primitive_range primitive_range_intersect(
    primitive_range a,
    primitive_range b);

/*
 * Take the inversion of a range.
 * 
 * Each matrix that is selected in the input is not selected in the
 * output.  Each matrix that is not selected in the input is selected in
 * the output.
 * 
 * Each articulation that is selected in the input is not selected in
 * the output.  Each articulation that is not selected in the input is
 * selected in the output.
 * 
 * Parameters:
 * 
 *   r - the operand
 * 
 * Return:
 * 
 *   the result
 */
primitive_range primitive_range_invert(primitive_range r);

/*
 * Set or clear a particular articulation in the given range.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   r - the input range
 * 
 *   art - the index of the articulation to modify, in range 0 to 61
 *   inclusive
 * 
 *   val - 1 if the articulation flag should be set, 0 to clear
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the result
 */
primitive_range primitive_range_art(
    primitive_range r,
    int32_t         art,
    int32_t         val,
    long            lnum);

/*
 * Set or clear a particular matrix in the given range.
 * 
 * This can be used to set the primary matrix, the secondary matrix, or
 * both at the same time.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   r - the input range
 * 
 *   mat - the matrix to modify, 1 is primary, 2 is secondary, 3 is both
 *   matrices
 * 
 *   val - 1 if the matrix flag(s) should be set, 0 to clear
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the result
 */
primitive_range primitive_range_matrix(
    primitive_range r,
    int32_t         mat,
    int32_t         val,
    long            lnum);

/*
 * Take the union of two bitmaps.
 * 
 * Channels are selected if they are present in either or both of the
 * operands.
 * 
 * Parameters:
 * 
 *   a - the first operand
 * 
 *   b - the second operand
 * 
 * Return:
 * 
 *   the result
 */
primitive_bitmap primitive_bitmap_union(
    primitive_bitmap a,
    primitive_bitmap b);

/*
 * Take the intersection of two bitmaps.
 * 
 * Channels are selected if they are present in both of the operands.
 * 
 * Parameters:
 * 
 *   a - the first operand
 * 
 *   b - the second operand
 * 
 * Return:
 * 
 *   the result
 */
primitive_bitmap primitive_bitmap_intersect(
    primitive_bitmap a,
    primitive_bitmap b);

/*
 * Take the inversion of a bitmap.
 * 
 * Each channel that is selected in the input is not selected in the
 * output.  Each channel that is not selected in the input is selected
 * in the output.
 * 
 * Parameters:
 * 
 *   r - the operand
 * 
 * Return:
 * 
 *   the result
 */
primitive_bitmap primitive_bitmap_invert(primitive_bitmap r);

/*
 * Set or clear a particular channel in the given bitmap.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   r - the input bitmap
 * 
 *   ch - the index of the channel to modify, in range 1 to 16 inclusive
 * 
 *   val - 1 if the channel flag should be set, 0 to clear
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the result
 */
primitive_bitmap primitive_bitmap_set(
    primitive_bitmap r,
    int32_t          ch,
    int32_t          val,
    long             lnum);

#endif
