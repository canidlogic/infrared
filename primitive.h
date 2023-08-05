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

#endif
