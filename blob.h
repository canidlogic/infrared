#ifndef BLOB_H_INCLUDED
#define BLOB_H_INCLUDED

/*
 * blob.h
 * ======
 * 
 * Blob manager of Infrared.
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
 * Maximum length in bytes of blob data.
 */
#define BLOB_MAXLEN INT32_C(1048576)

/*
 * Type declarations
 * =================
 */

/*
 * BLOB structure prototype.
 * 
 * See the implementation file for definition.
 */
struct BLOB_TAG;
typedef struct BLOB_TAG BLOB;

/*
 * Public functions
 * ================
 */

/*
 * Create a new blob object using a base-16 string to specify its
 * contents.
 * 
 * pStr is the nul-terminated base-16 string.  It may only contain ASCII
 * base-16 characters (both letter cases acceptable), spaces, tabs, CRs,
 * and LFs.  Space, tab, CR, and LF are all considered whitespace
 * characters.
 * 
 * A base-16 string contains zero or more byte declarations.  Each byte
 * declaration begins with an optional sequence of zero or more
 * whitespace characters, followed by exactly two base-16 digits using
 * either letter case.  After the last byte declaration comes an
 * optional sequence of zero or more whitespace characters.
 * 
 * Empty base-16 strings and blank base-16 strings with only whitespace
 * are allowed, and result in empty blobs.  The maximum number of byte
 * declarations that can be present is limited by BLOB_MAXLEN.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pStr - the base-16 string to parse for blob data
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the new blob
 */
BLOB *blob_fromHex(const char *pStr, long lnum);

/*
 * Create a new blob object by concatenating together existing blob
 * objects.
 * 
 * ppList is the pointer to the list of component blobs to join, which
 * may be NULL only if list_len is zero.
 * 
 * list_len is the number of component blobs in the list, which must be
 * zero or greater.
 * 
 * The total concatenated length must be within the BLOB_MAXLEN limit or
 * there will be an error.
 * 
 * The given lnum should be from the Shastina parser, for use in error
 * reports.
 * 
 * Parameters:
 * 
 *   ppList - the list of component blobs or NULL if empty
 * 
 *   list_len - the number of component blob handles in the list
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the new blob
 */
BLOB *blob_concat(BLOB **ppList, int32_t list_len, long lnum);

/*
 * Create a new blob as a subrange from another.
 * 
 * pSrc is the source blob that the new blob is derived from.
 * 
 * i and j specify the subrange.  i is the offset of the first byte that
 * will be included in the subrange.  j is the offset that is one beyond
 * the last byte that will be included in the subrange.  (j - i) is
 * equal to the length in bytes of the new blob.
 * 
 * i must be greater than or equal to zero and less than or equal to the
 * length of the source blob.  j must be greater than or equal to i and
 * less than or equal to the length of the source blob.  If j is equal
 * to i, then the new blob will be empty.
 * 
 * Parameters:
 * 
 *   pSrc - the source blob
 * 
 *   i - the index of the first byte
 * 
 *   j - the index one beyond the last byte
 * 
 *   lnum - the Shastina line number for diagnostic messages
 * 
 * Return:
 * 
 *   the new blob
 */
BLOB *blob_slice(BLOB *pSrc, int32_t i, int32_t j, long lnum);

/*
 * Shut down the blob system, releasing all blobs that have been
 * allocated and preventing all further calls to blob functions, except
 * for blob_shutdown().
 */
void blob_shutdown(void);

/*
 * Get a pointer to the data within a given blob.
 * 
 * Zero bytes may be part of the data, so you can NOT treat the returned
 * binary string as nul-terminated.  The pointer remains valid until the
 * blob_shutdown() function is called.
 * 
 * The return value may be NULL if blob_len() indicates an empty blob.
 * 
 * Parameters:
 * 
 *   pb - the blob
 * 
 * Return:
 * 
 *   pointer to the blob data, or NULL if blob data is empty
 */
const uint8_t *blob_ptr(BLOB *pb);

/*
 * Determine the length in bytes of a given blob.
 * 
 * The length may be in range zero to BLOB_MAXLEN, inclusive.
 * 
 * Parameters:
 * 
 *   pb - the blob
 * 
 * Return:
 * 
 *   the length of the blob data in bytes
 */
int32_t blob_len(BLOB *pb);

#endif
