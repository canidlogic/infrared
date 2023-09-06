#ifndef TEXT_H_INCLUDED
#define TEXT_H_INCLUDED

/*
 * text.h
 * ======
 * 
 * Text manager of Infrared.
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
 * Maximum length in bytes of text data, not including terminating nul.
 */
#define TEXT_MAXLEN INT32_C(1023)

/*
 * Type declarations
 * =================
 */

/*
 * TEXT structure prototype.
 * 
 * See the implementation file for definition.
 */
struct TEXT_TAG;
typedef struct TEXT_TAG TEXT;

/*
 * Public functions
 * ================
 */

/*
 * Create a new text object given a literal string containing its
 * contents.
 * 
 * pStr is the nul-terminated source string.  It may only contain
 * visible, printing ASCII characters and the ASCII space.  Empty
 * strings are acceptable.  The maximum string length (excluding the
 * terminating nul) is TEXT_MAXLEN.  A copy will be made of this string
 * in the created text object.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pStr - the literal ASCII string to copy
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the new text
 */
TEXT *text_literal(const char *pStr, long lnum);

/*
 * Create a new text object by concatenating together existing text
 * objects.
 * 
 * ppList is the pointer to the list of component texts to join, which
 * may be NULL only if list_len is zero.
 * 
 * list_len is the number of component texts in the list, which must be
 * zero or greater.
 * 
 * The total concatenated length must be within the TEXT_MAXLEN limit or
 * there will be an error.
 * 
 * The given lnum should be from the Shastina parser, for use in error
 * reports.
 * 
 * Parameters:
 * 
 *   ppList - the list of component texts or NULL if empty
 * 
 *   list_len - the number of component text handles in the list
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the new text
 */
TEXT *text_concat(TEXT **ppList, int32_t list_len, long lnum);

/*
 * Create a new text as a subrange from another.
 * 
 * pSrc is the source text that the new text is derived from.
 * 
 * i and j specify the subrange.  i is the offset of the first character
 * that will be included in the subrange.  j is the offset that is one
 * beyond the last character that will be included in the subrange.
 * (j - i) is equal to the length in characters of the new text,
 * excluding the terminating nul.
 * 
 * i must be greater than or equal to zero and less than or equal to the
 * length of the source text, excluding the terminating nul.  j must be
 * greater than or equal to i and less than or equal to the length of
 * the source text, excluding the terminating nul.  If j is equal to i,
 * then the new text will be empty.
 * 
 * Parameters:
 * 
 *   pSrc - the source text
 * 
 *   i - the index of the first character
 * 
 *   j - the index one beyond the last character
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the new text
 */
TEXT *text_slice(TEXT *pSrc, int32_t i, int32_t j, long lnum);

/*
 * Shut down the text system, releasing all texts that have been
 * allocated and preventing all further calls to text functions, except
 * for text_shutdown().
 */
void text_shutdown(void);

/*
 * Get a pointer to the string within a given text.
 * 
 * The string will be nul-terminated.  The returned pointer will never
 * be NULL.  The pointer remains valid until the text_shutdown()
 * function is called.
 * 
 * Parameters:
 * 
 *   pt - the text
 * 
 * Return:
 * 
 *   pointer to the nul-terminated string
 */
const char *text_ptr(TEXT *pt);

/*
 * Determine the length in characters of a given text.
 * 
 * The length may be in range zero to TEXT_MAXLEN, inclusive.  It does
 * not include the terminating nul.
 * 
 * Parameters:
 * 
 *   pt - the text
 * 
 * Return:
 * 
 *   the length of the text data in bytes
 */
int32_t text_len(TEXT *pt);

#endif
