/*
 * midi.c
 * ======
 * 
 * Implementation of midi.h
 * 
 * See header for further information.
 */

#include "midi.h"

#include <limits.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "diagnostic.h"
#include "pointer.h"

/*
 * Diagnostics
 * ===========
 */

static void raiseErr(int lnum, const char *pDetail, ...) {
  va_list ap;
  va_start(ap, pDetail);
  diagnostic_global(1, __FILE__, lnum, pDetail, ap);
  va_end(ap);
}

static void sayWarn(int lnum, const char *pDetail, ...) {
  va_list ap;
  va_start(ap, pDetail);
  diagnostic_global(0, __FILE__, lnum, pDetail, ap);
  va_end(ap);
}

/*
 * Constants
 * =========
 */

/*
 * The initial and maximum capacities of the handle table.
 */
#define H_INIT_CAP  (16)
#define H_MAX_CAP   (16384)

/*
 * The initial and maximum capacities of the message buffer.
 * 
 * The maximum capacity can't go beyond 16777215 because selectors only
 * have 24 bits for byte offsets in the message buffer.
 */
#define MSG_INIT_CAP  (4096)
#define MSG_MAX_CAP   INT32_C(16777215)

/*
 * The initial and maximum capacities of the header buffer.
 */
#define HEAD_INIT_CAP (16)
#define HEAD_MAX_CAP  (16384)

/*
 * The initial and maximum capacities of the moment buffer.
 */
#define MOMENT_INIT_CAP (1024)
#define MOMENT_MAX_CAP  INT32_C(8388608)

/*
 * Type declarations
 * =================
 */

/*
 * Record format for the handle table.
 */
typedef struct {
  
  /*
   * Non-zero if this is a blob, or zero if this is text.
   */
  int is_blob;
  
  /*
   * The pointer value, which is either a blob or text pointer depending
   * on the is_blob field.
   */
  union {
    BLOB *pBlob;
    TEXT *pText;
  } ptr;
  
} HANDLE_ENTRY;

/*
 * Record format for the moment buffer.
 */
typedef struct {
  
  /*
   * The event ID.
   * 
   * Event IDs are unique to events and are assigned in ascending order
   * matching the order in which events were entered into the MIDI
   * module.
   */
  int32_t eid;
  
  /*
   * The moment offset of the event.
   */
  int32_t t;
  
  /*
   * The message selector.
   * 
   * The most significant 8 bits are the status byte of the MIDI
   * message.  The lower 24 bits are a byte offset into the byte buffer.
   */
  uint32_t sel;
  
} MOMENT;

/*
 * Local data
 * ==========
 */

/*
 * Flag indicating whether the compilation function has been called yet.
 */
static int m_compiled = 0;

/*
 * The handle table.
 * 
 * Messages that include blob or text data indirectly reference this
 * data in the message buffer as offsets into this table.  Each entry
 * refers either to a blob or a text object.
 * 
 * The fields here are the current capacity as a record count, the
 * number of records actually in use, and a pointer to the dynamically
 * allocated record array.
 */
static int32_t m_h_cap = 0;
static int32_t m_h_len = 0;
static HANDLE_ENTRY *m_h = NULL;

/*
 * The event ID assignment variable.
 * 
 * Each time an event ID is assigned, this variable is incremented and
 * the incremented value is the new event ID.  Therefore, the first
 * assigned event ID will be 1.
 */
static int32_t m_unique = 0;

/*
 * The lower and upper boundaries of the event range, as subquantum
 * offsets.
 * 
 * This is the range of all events that have been defined in the moment
 * buffer as well as null events targeted at the moment buffer.
 * 
 * If m_filled is zero, it means that no events have been defined yet
 * and the boundaries are both just zero in this case.  If m_filled is
 * non-zero, it means at least one event has been defined, and the
 * boundaries apply to all events defined so far.
 */
static int m_filled = 0;
static int32_t m_lower = 0;
static int32_t m_upper = 0;

/*
 * The message buffer.
 * 
 * This contains message data that is referred to from selectors in the
 * header buffer and moment buffer.  The selectors store a byte offset
 * into this buffer in their 24 least significant bits.
 * 
 * The format of the data in the message buffer depends on the status
 * byte that is stored in the 8 most significant bits of the selector.
 * 
 * For status bytes in range 0x80 to 0xBF inclusive and 0xE0 to 0xEF
 * inclusive, there are two data bytes in the message buffer that should
 * be appended to the status byte in the selector.
 * 
 * For status bytes in range 0xC0 to 0xDF inclusive, there is one data
 * byte in the message buffer that should be appended to the status byte
 * in the selector.
 * 
 * For status bytes 0xF0 and 0xF7, the byte buffer contains a MIDI
 * variable-length integer that encodes an index in the handle table
 * that must be for a blob.  For status byte 0xF0, the blob may not be
 * empty and its first data byte must be 0xF0.
 * 
 * For status byte 0xFF, the first data byte is the meta-event type
 * code along with a data type flag in the most significant bit.  If the
 * data type flag is clear, then after the first data byte comes a MIDI
 * variable-length integer that encodes the payload length in bytes, and
 * after that comes the message payload.  If the data type flag is set,
 * then after the first data byte comes a MIDI variable-length integer
 * that encodes an index in the handle table that may be either for a
 * text or a blob.
 */
static int32_t m_msg_cap = 0;
static int32_t m_msg_len = 0;
static uint8_t *m_msg = NULL;

/*
 * The header buffer.
 * 
 * Header events are always output first in the MIDI file track at time
 * zero, before any moment buffer events, and in the order they were
 * entered into the header buffer.
 * 
 * Each header entry is a selector with the same format as the sel field
 * in the MOMENT structure.  In other words, the header table has MOMENT
 * structures without any timing information.
 * 
 * The fields here are the current capacity as a record count, the
 * number of records actually in use, and a pointer to the dynamically
 * allocated record array.
 */
static int32_t m_head_cap = 0;
static int32_t m_head_len = 0;
static uint32_t *m_head = NULL;

/*
 * The moment buffer.
 * 
 * Be sure to update the m_lower and m_upper event range values when
 * entering events into this buffer, and also to set m_filled.
 * 
 * The fields here are the current capacity as a record count, the
 * number of records actually in use, and a pointer to the dynamically
 * allocated record array.
 */
static int32_t m_moment_cap = 0;
static int32_t m_moment_len = 0;
static MOMENT *m_moment = NULL;

/*
 * The running status byte.
 * 
 * Zero if there is no running status byte, otherwise the running status
 * byte.  Used by the printMsg() function.
 * 
 * Only status bytes in range 0x80 to 0xEF inclusive may be used in
 * running status.  Any other status bytes clear running status.
 */
static int m_rstatus = 0;

/*
 * Local functions
 * ===============
 */

/* Prototypes */
static void writeByte(FILE *pOut, int c);
static void writeBinary(FILE *pOut, const uint8_t *pData, int32_t len);

static void writeString(FILE *pOut, const char *pStr);
static void writeUint32BE(FILE *pOut, uint32_t val);
static void writeUint16BE(FILE *pOut, uint16_t val);

static int encodeVInt(uint8_t *pBuf, int32_t val);
static int decodeVInt(const uint8_t *pBuf, int32_t *pVal, int32_t lim);
static void printVInt(FILE *pOut, int32_t val);

static void capH(int32_t n);
static int32_t addBlobH(BLOB *pBlob);
static int32_t addTextH(TEXT *pText);

static int32_t eventID(void);
static void eventRange(int32_t t);

static void capMsg(int32_t n);
static uint32_t addMsg1(int status, int b);
static uint32_t addMsg2(int status, int b1, int b2);
static uint32_t addMsgB(int status, BLOB *pBlob);
static uint32_t addMsgMB(int status, int ty, BLOB *pBlob);
static uint32_t addMsgMT(int status, int ty, TEXT *pText);
static uint32_t addMsgMD(
          int       status,
          int       ty,
    const uint8_t * pData,
          int32_t   len);

static void printMsg(FILE *pOut, uint32_t sel);
static int32_t sizeMsg(uint32_t sel, uint32_t prev);

static void capHead(int32_t n);
static void addHeadMsg(uint32_t sel);

static void capMoment(int32_t n);
static void addMomentMsg(int32_t t, uint32_t sel);

static int cmpMoment(const void *pA, const void *pB);

/*
 * Write an unsigned byte value to the output file.
 * 
 * This is one of the two low-level output functions, the other being
 * writeBinary().
 * 
 * Parameters:
 * 
 *   pOut - the file
 *   
 *   c - the unsigned byte value (0-255)
 */
static void writeByte(FILE *pOut, int c) {
  if (pOut == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if ((c < 0) || (c > 255)) {
    raiseErr(__LINE__, NULL);
  }
  if (putc(c, pOut) != c) {
    raiseErr(__LINE__, "I/O error during output");
  }
}

/*
 * Write a binary string to the output file.
 * 
 * This is one of the two low-level output functions, the other being
 * writeByte().
 * 
 * The length in bytes must be zero or greater.  If zero, the function
 * does nothing.
 * 
 * Parameters:
 * 
 *   pOut - the file
 * 
 *   pData - the data to write
 * 
 *   len - the number of bytes to write
 */
static void writeBinary(FILE *pOut, const uint8_t *pData, int32_t len) {
  if (len < 0) {
    raiseErr(__LINE__, NULL);
  }
  if (len > 0) {
    if ((pOut == NULL) || (pData == NULL)) {
      raiseErr(__LINE__, NULL);
    }
    if (fwrite(pData, 1, (size_t) len, pOut) != len) {
      raiseErr(__LINE__, "I/O error during output");
    }
  }
}

/*
 * Write a nul-terminated string to the output file, not including the
 * terminating nul.
 * 
 * This is a wrapper around writeBinary().  If the string starts
 * immediately with the terminating nul, nothing is written.
 * 
 * Parameters:
 * 
 *   pOut - the file
 * 
 *   pStr - the nul-terminated string
 */
static void writeString(FILE *pOut, const char *pStr) {
  if ((pOut == NULL) || (pStr == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  writeBinary(pOut, (const uint8_t *) pStr, (int32_t) strlen(pStr));
}

/*
 * Write a 32-bit unsigned integer to the output file in big endian
 * order.
 * 
 * This is a wrapper around writeByte().
 * 
 * Parameters:
 * 
 *   pOut - the file
 * 
 *   val - the value to write
 */
static void writeUint32BE(FILE *pOut, uint32_t val) {
  
  int c1 = 0;
  int c2 = 0;
  int c3 = 0;
  int c4 = 0;
  
  if (pOut == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  c1 = (int)  (val >> 24)        ;
  c2 = (int) ((val >> 16) & 0xff);
  c3 = (int) ((val >>  8) & 0xff);
  c4 = (int) ( val        & 0xff);
  
  writeByte(pOut, c1);
  writeByte(pOut, c2);
  writeByte(pOut, c3);
  writeByte(pOut, c4);
}

/*
 * Write a 16-bit unsigned integer to the output file in big endian
 * order.
 * 
 * This is a wrapper around writeByte().
 * 
 * Parameters:
 * 
 *   pOut - the file
 * 
 *   val - the value to write
 */
static void writeUint16BE(FILE *pOut, uint16_t val) {
  
  int c1 = 0;
  int c2 = 0;
  
  if (pOut == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  c1 = (int) (val >>    8);
  c2 = (int) (val &  0xff);
  
  writeByte(pOut, c1);
  writeByte(pOut, c2);
}

/*
 * Encode an unsigned integer value into a MIDI variable-length integer.
 * 
 * The given integer value must be in range zero to 0x0FFFFFFF.  It will
 * be encoded into one to four bytes, depending on the range.  Each byte
 * stores seven data bits.  The most significant bit is set for all data
 * bytes except the last one.  The bytes are ordered in big endian.
 * 
 * Make sure the pBuf points to a buffer of at least four bytes.  The
 * return value indicates how many bytes were written.
 * 
 * Parameters:
 * 
 *   pBuf - the buffer to write the encoded bytes to
 * 
 *   val - the value to encode
 * 
 * Return:
 * 
 *   the number of encoded bytes, in range 1 to 4 inclusive
 */
static int encodeVInt(uint8_t *pBuf, int32_t val) {
  
  int count = 0;
  
  if (pBuf == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if ((val < 0) || (val > INT32_C(0x0FFFFFFF))) {
    raiseErr(__LINE__, NULL);
  }
  
  if (val >= INT32_C(0x00200000)) {
    *pBuf = (uint8_t) (((val >> 21) & 0x7f) | 0x80);
    pBuf++;
    count++;
  }
  
  if (val >= INT32_C(0x00004000)) {
    *pBuf = (uint8_t) (((val >> 14) & 0x7f) | 0x80);
    pBuf++;
    count++;
  }
  
  if (val >= INT32_C(0x00000080)) {
    *pBuf = (uint8_t) (((val >>  7) & 0x7f) | 0x80);
    pBuf++;
    count++;
  }
  
  *pBuf = (uint8_t) (val & 0x7f);
  pBuf++;
  count++;
  
  return count;
}

/*
 * Decode a MIDI variable-length integer into an unsigned integer value.
 * 
 * The variable-length integer format is the same format that is
 * produced by encodeVInt().  See that function for a specification.
 * 
 * There may be at most four encoded bytes.  An error occurs if the
 * encoded representation is greater than four bytes.
 * 
 * lim is the number of bytes remaining in the buffer pointed to by
 * pBuf.  An error occurs if the buffer runs out before a full
 * variable-length integer has been decoded.  An error will always occur
 * if a value less than one is passed.
 * 
 * Parameters:
 * 
 *   pBuf - the variable-length integer to decode
 * 
 *   pVal - receives the decoded value, or NULL if not needed
 * 
 *   lim - the number of bytes remaining in the buffer
 * 
 * Return:
 * 
 *   the number of encoded bytes in the representation, in range 1 to 4
 *   inclusive
 */
static int decodeVInt(const uint8_t *pBuf, int32_t *pVal, int32_t lim) {
  
  int32_t result = 0;
  int count = 0;
  
  if (pBuf == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if (lim < 1) {
    raiseErr(__LINE__, NULL);
  }
  
  for( ; (*pBuf & 0x80) != 0; pBuf++) {
    count++;
    if ((count >= lim) || (count > 3)) {
      raiseErr(__LINE__, NULL);
    }
    result = (result << 7) | ((int32_t) (*pBuf & 0x7f));
  }
  
  result = (result << 7) | ((int32_t) *pBuf);
  count++;
  
  if (pVal != NULL) {
    *pVal = result;
  }
  
  return count;
}

/*
 * Determine the encoded size in bytes of the MIDI variable-length
 * integer representation of a given integer value.
 * 
 * The given integer value must be in range zero to 0x0FFFFFFF.  This
 * function returns the same encoded byte count that would be returned
 * when using encodeVInt() on this integer value.
 * 
 * Parameters:
 * 
 *   val - the integer value that will be encoded
 * 
 * Return:
 * 
 *   the number of encoded bytes, in range 1 to 4 inclusive
 */
static int sizeVInt(int32_t val) {
  
  int result = 0;
  
  if ((val < 0) || (val > INT32_C(0x0FFFFFFF))) {
    raiseErr(__LINE__, NULL);
  }
  
  if (val >= INT32_C(0x00200000)) {
    result = 4;
  
  } else if (val >= INT32_C(0x00004000)) {
    result = 3;
  
  } else if (val >= INT32_C(0x00000080)) {
    result = 2;
  
  } else {
    result = 1;
  }
  
  return result;
}

/*
 * Print an encoded MIDI variable-length integer to an output file.
 * 
 * The given integer value must be in range zero to 0x0FFFFFFF.  This
 * function is a wrapper around encodeVInt() and writeBinary().
 * 
 * Parameters:
 * 
 *   pOut - the file to write
 * 
 *   val - the integer value to encode
 */
static void printVInt(FILE *pOut, int32_t val) {
  
  uint8_t buf[4];
  int len = 0;
  
  memset(buf, 0, 4);
  
  if (pOut == NULL) {
    raiseErr(__LINE__, NULL);
  }
  if ((val < 0) || (val > INT32_C(0x0FFFFFFF))) {
    raiseErr(__LINE__, NULL);
  }
  
  len = encodeVInt(buf, val);
  writeBinary(pOut, buf, (int32_t) len);
}

/*
 * Make room in capacity for a given number of elements in the handle
 * table.
 * 
 * n is the number of additional elements beyond current length to make
 * room for.  It must be zero or greater.
 * 
 * Upon return, the capacity will be at least n elements higher than the
 * current length.
 * 
 * An error occurs if the requested expansion would go beyond the
 * maximum allowed capacity.
 * 
 * Parameters:
 * 
 *   n - the number of elements to make room for
 */
static void capH(int32_t n) {
  
  int32_t target = 0;
  int32_t new_cap = 0;
  
  /* Check parameters */
  if (n < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Only proceed if n is non-zero */
  if (n > 0) {
    
    /* Make initial allocation if necessary */
    if (m_h_cap < 1) {
      m_h = (HANDLE_ENTRY *) calloc(
              (size_t) H_INIT_CAP, sizeof(HANDLE_ENTRY));
      if (m_h == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      m_h_cap = H_INIT_CAP;
      m_h_len = 0;
    }
    
    /* Compute target length */
    if (n <= INT32_MAX - m_h_len) {
      target = m_h_len + n;
    } else {
      raiseErr(__LINE__, "MIDI handle table capacity exceeded");
    }
    
    /* Only proceed if target length exceeds current capacity */
    if (target > m_h_cap) {
      /* Check that target within maximum capacity */
      if (target > H_MAX_CAP) {
        raiseErr(__LINE__, "MIDI handle table capacity exceeded");
      }
      
      /* Compute new capacity by doubling current capacity until greater
       * than or equal to target length, and then limiting to maximum
       * capacity */
      new_cap = m_h_cap;
      while (new_cap < target) {
        new_cap *= 2;
      }
      if (new_cap > H_MAX_CAP) {
        new_cap = H_MAX_CAP;
      }
      
      /* Expand capacity */
      m_h = (HANDLE_ENTRY *) realloc(m_h,
                            ((size_t) new_cap) * sizeof(HANDLE_ENTRY));
      if (m_h == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      memset(
        &(m_h[m_h_cap]),
        0,
        ((size_t) (new_cap - m_h_cap)) * sizeof(HANDLE_ENTRY));
      
      m_h_cap = new_cap;
    }
  }
}

/*
 * Add a blob handle to the handle table and return the handle table
 * index of the blob that was just added.
 * 
 * Parameters:
 * 
 *   pBlob - the blob to add
 * 
 * Return:
 * 
 *   the handle table index of the blob
 */
static int32_t addBlobH(BLOB *pBlob) {
  if (pBlob == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  capH(1);
  
  (m_h[m_h_len]).is_blob = 1;
  (m_h[m_h_len]).ptr.pBlob = pBlob;
  m_h_len++;
  
  return (m_h_len - 1);
}

/*
 * Add a text handle to the handle table and return the handle table
 * index of the text that was just added.
 * 
 * Parameters:
 * 
 *   pText - the text to add
 * 
 * Return:
 * 
 *   the handle table index of the text
 */
static int32_t addTextH(TEXT *pText) {
  if (pText == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  capH(1);
  
  (m_h[m_h_len]).is_blob = 0;
  (m_h[m_h_len]).ptr.pText = pText;
  m_h_len++;
  
  return (m_h_len - 1);
}

/*
 * Return a newly generated event ID.
 * 
 * The first generated event ID is 1 and all subsequent calls are one
 * greater than the previous return value.  An error occurs if the range
 * of the 32-bit integer would be exceeded.
 * 
 * Return:
 * 
 *   a new event ID
 */
static int32_t eventID(void) {
  if (m_unique < INT32_MAX) {
    m_unique++;
  } else {
    raiseErr(__LINE__, "Event ID generation overflow");
  }
  return m_unique;
}

/*
 * Expand the event range if necessary to include the given moment
 * offset.
 * 
 * The moment offset will automatically be converted into a subquantum
 * offset.
 * 
 * Parameters:
 * 
 *   t - the moment offset to include
 */
static void eventRange(int32_t t) {
  t = pointer_unpack(t, NULL);
  
  if (m_filled) {
    if (t < m_lower) {
      m_lower = t;
    
    } else if (t > m_upper) {
      m_upper = t;
    }
    
  } else {
    m_filled = 1;
    m_lower = t;
    m_upper = t;
  }
}

/*
 * Make room in capacity for a given number of bytes in the message
 * table.
 * 
 * n is the number of additional elements beyond current length to make
 * room for.  It must be zero or greater.
 * 
 * Upon return, the capacity will be at least n elements higher than the
 * current length.
 * 
 * An error occurs if the requested expansion would go beyond the
 * maximum allowed capacity.
 * 
 * Parameters:
 * 
 *   n - the number of elements to make room for
 */
static void capMsg(int32_t n) {
  
  int32_t target = 0;
  int32_t new_cap = 0;
  
  /* Check parameters */
  if (n < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Only proceed if n is non-zero */
  if (n > 0) {
    
    /* Make initial allocation if necessary */
    if (m_msg_cap < 1) {
      m_msg = (uint8_t *) calloc((size_t) MSG_INIT_CAP, 1);
      if (m_msg == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      m_msg_cap = MSG_INIT_CAP;
      m_msg_len = 0;
    }
    
    /* Compute target length */
    if (n <= INT32_MAX - m_msg_len) {
      target = m_msg_len + n;
    } else {
      raiseErr(__LINE__, "MIDI message table capacity exceeded");
    }
    
    /* Only proceed if target length exceeds current capacity */
    if (target > m_msg_cap) {
      /* Check that target within maximum capacity */
      if (target > MSG_MAX_CAP) {
        raiseErr(__LINE__, "MIDI message table capacity exceeded");
      }
      
      /* Compute new capacity by doubling current capacity until greater
       * than or equal to target length, and then limiting to maximum
       * capacity */
      new_cap = m_msg_cap;
      while (new_cap < target) {
        new_cap *= 2;
      }
      if (new_cap > MSG_MAX_CAP) {
        new_cap = MSG_MAX_CAP;
      }
      
      /* Expand capacity */
      m_msg = (uint8_t *) realloc(m_msg, (size_t) new_cap);
      if (m_msg == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      memset(
        &(m_msg[m_msg_cap]),
        0,
        (size_t) (new_cap - m_msg_cap));
      
      m_msg_cap = new_cap;
    }
  }
}

/*
 * Add a MIDI message that has a single data byte to the message buffer
 * and return a selector for this MIDI message.
 * 
 * The supported status bytes are in range 0xC0 to 0xDF inclusive.  The
 * data byte must be in range 0 to 127 inclusive.
 * 
 * Parameters:
 * 
 *   status - the status byte
 * 
 *   b - the data byte
 * 
 * Return:
 * 
 *   the selector
 */
static uint32_t addMsg1(int status, int b) {
  
  uint32_t sel = 0;
  
  if ((status < 0xc0) || (status > 0xdf)) {
    raiseErr(__LINE__, NULL);
  }
  if ((b < 0) || (b > 127)) {
    raiseErr(__LINE__, NULL);
  }
  
  capMsg(1);
  
  sel = (uint32_t) m_msg_len;
  m_msg[sel] = (uint8_t) b;
  m_msg_len++;
  
  sel |= (((uint32_t) status) << 24);
  
  return sel;
}

/*
 * Add a MIDI message that has two data bytes to the message buffer and
 * return a selector for this MIDI message.
 * 
 * The supported status bytes are in range 0x80 to 0xBF, and 0xE0 to
 * 0xEF inclusive.  The data bytes must be in range 0 to 127 inclusive.
 * 
 * Parameters:
 * 
 *   status - the status byte
 * 
 *   b1 - the first data byte
 * 
 *   b2 - the second data byte
 * 
 * Return:
 * 
 *   the selector
 */
static uint32_t addMsg2(int status, int b1, int b2) {
  
  uint32_t sel = 0;
  
  if (((status < 0x80) || (status > 0xbf)) &&
      ((status < 0xe0) || (status > 0xef))) {
    raiseErr(__LINE__, NULL);
  }
  if ((b1 < 0) || (b1 > 127) ||
      (b2 < 0) || (b2 > 127)) {
    raiseErr(__LINE__, NULL);
  }
  
  capMsg(2);
  
  sel = (uint32_t) m_msg_len;
  m_msg[sel    ] = (uint8_t) b1;
  m_msg[sel + 1] = (uint8_t) b2;
  m_msg_len += 2;
  
  sel |= (((uint32_t) status) << 24);
  
  return sel;
}

/*
 * Add a MIDI message that has blob data to the message buffer and
 * return a selector for this MIDI message.
 * 
 * The supported status bytes are only 0xF0 and 0xF7.  If the status
 * byte is 0xF0, then the blob must not be empty and its first byte must
 * be 0xF0.
 * 
 * Parameters:
 * 
 *   status - the status byte
 * 
 *   pBlob - the blob
 * 
 * Return:
 * 
 *   the selector
 */
static uint32_t addMsgB(int status, BLOB *pBlob) {
  
  int32_t bh = 0;
  uint32_t sel = 0;
  
  if ((status != 0xf0) && (status != 0xf7)) {
    raiseErr(__LINE__, NULL);
  }
  if (pBlob == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  if (status == 0xf0) {
    if (blob_len(pBlob) < 1) {
      raiseErr(__LINE__, NULL);
    }
    if ((blob_ptr(pBlob))[0] != 0xf0) {
      raiseErr(__LINE__, NULL);
    }
  }
  
  bh = addBlobH(pBlob);
  
  capMsg((int32_t) sizeVInt(bh));
  
  sel = (uint32_t) m_msg_len;
  m_msg_len += (int32_t) encodeVInt(&(m_msg[sel]), bh);
  
  sel |= (((uint32_t) status) << 24);
  
  return sel;
}

/*
 * Add a MIDI message that has a meta-event type code and blob data to
 * the message buffer and return a selector for this MIDI message.
 * 
 * The only supported status byte is 0xff.  The type code must be in
 * range 0 to 127 inclusive.
 * 
 * Parameters:
 * 
 *   status - the status byte
 * 
 *   ty - the meta-event type code
 * 
 *   pBlob - the blob
 * 
 * Return:
 * 
 *   the selector
 */
static uint32_t addMsgMB(int status, int ty, BLOB *pBlob) {
  
  int32_t bh = 0;
  uint32_t sel = 0;
  
  if (status != 0xff) {
    raiseErr(__LINE__, NULL);
  }
  if ((ty < 0) || (ty > 127)) {
    raiseErr(__LINE__, NULL);
  }
  if (pBlob == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  bh = addBlobH(pBlob);
  
  capMsg((int32_t) (sizeVInt(bh) + 1));
  
  sel = (uint32_t) m_msg_len;
  m_msg[sel] = (uint8_t) (ty | 0x80);
  m_msg_len++;
  
  m_msg_len += (int32_t) encodeVInt(&(m_msg[sel + 1]), bh);
  
  sel |= (((uint32_t) status) << 24);
  
  return sel;
}

/*
 * Add a MIDI message that has a meta-event type code and text data to
 * the message buffer and return a selector for this MIDI message.
 * 
 * The only supported status byte is 0xff.  The type code must be in
 * range 0 to 127 inclusive.
 * 
 * Parameters:
 * 
 *   status - the status byte
 * 
 *   ty - the meta-event type code
 * 
 *   pText - the text
 * 
 * Return:
 * 
 *   the selector
 */
static uint32_t addMsgMT(int status, int ty, TEXT *pText) {
  
  int32_t th = 0;
  uint32_t sel = 0;
  
  if (status != 0xff) {
    raiseErr(__LINE__, NULL);
  }
  if ((ty < 0) || (ty > 127)) {
    raiseErr(__LINE__, NULL);
  }
  if (pText == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  th = addTextH(pText);
  
  capMsg((int32_t) (sizeVInt(th) + 1));
  
  sel = (uint32_t) m_msg_len;
  m_msg[sel] = (uint8_t) (ty | 0x80);
  m_msg_len++;
  
  m_msg_len += (int32_t) encodeVInt(&(m_msg[sel + 1]), th);
  
  sel |= (((uint32_t) status) << 24);
  
  return sel;
}

/*
 * Add a MIDI message that has a meta-event type code and binary data to
 * the message buffer and return a selector for this MIDI message.
 * 
 * The only supported status byte is 0xff.  The type code must be in
 * range 0 to 127 inclusive.
 * 
 * len is the length of binary data, not including the type code or the
 * length declaration.  If len is non-zero, pData is a non-NULL pointer
 * to the data, which is copied into the message buffer.  The length
 * declaration will be generated automatically and should not be present
 * in the passed buffer.
 * 
 * Parameters:
 * 
 *   status - the status byte
 * 
 *   ty - the meta-event type code
 * 
 *   pData - the payload data to copy, if len is non-zero
 * 
 *   len - the length in bytes of payload data
 * 
 * Return:
 * 
 *   the selector
 */
static uint32_t addMsgMD(
          int       status,
          int       ty,
    const uint8_t * pData,
          int32_t   len) {
  
  uint32_t sel = 0;
  
  if (status != 0xff) {
    raiseErr(__LINE__, NULL);
  }
  if ((ty < 0) || (ty > 127)) {
    raiseErr(__LINE__, NULL);
  }
  if (len < 0) {
    raiseErr(__LINE__, NULL);
  }
  if (len > 0) {
    if (pData == NULL) {
      raiseErr(__LINE__, NULL);
    }
  }
  
  if (len > INT32_C(0x0FFFFFFF)) {
    raiseErr(__LINE__, "MIDI message table capacity exceeded");
  }
  
  capMsg(len + ((int32_t) sizeVInt(len)) + 1);
  
  sel = (uint32_t) m_msg_len;
  m_msg[sel] = (uint8_t) ty;
  m_msg_len++;
  
  m_msg_len += (int32_t) encodeVInt(&(m_msg[sel + 1]), len);
  
  if (len > 0) {
    memcpy(
      &(m_msg[m_msg_len]),
      pData,
      (size_t) len);
    
    m_msg_len += len;
  }
  
  sel |= (((uint32_t) status) << 24);
  
  return sel;
}

/*
 * Print a MIDI message to the given output file.
 * 
 * sel is the selector of the MIDI message to print.  Only the MIDI
 * message is printed, not the delta time preceding it.
 * 
 * This function will use running status byte optimization.  Therefore,
 * it assumes that messages are being printed in sequential order.
 * Otherwise, the running status bytes won't work correctly.
 * 
 * Parameters:
 * 
 *   pOut - the output file
 * 
 *   sel - the selector of the MIDI message to print
 */
static void printMsg(FILE *pOut, uint32_t sel) {
  
  int status = 0;
  int proceed = 0;
  int ty = 0;
  int32_t msg = 0;
  int32_t h = 0;
  int32_t len = 0;
  int32_t cl = 0;
  
  /* Check parameters */
  if (pOut == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Parse selector into status and message buffer offset */
  status = (int) (sel >> 24);
  msg = (int32_t) (sel & UINT32_C(0xffffff));
  
  /* Determine whether to write the status byte -- write it in all cases
   * except when a status byte is buffered that equals the current
   * status byte */
  proceed = 1;
  if (m_rstatus != 0) {
    if (m_rstatus == status) {
      proceed = 0;
    }
  }
  
  /* Write status byte if necessary */
  if (proceed) {
    writeByte(pOut, status);
  }
  
  /* If status byte is in range 0x80 to 0xEF inclusive, then store it as
   * the running status state; otherwise, clear running status state */
  if ((status >= 0x80) && (status <= 0xef)) {
    m_rstatus = status;
  } else {
    m_rstatus = 0;
  }
  
  /* Check that message offset is in range of message buffer */
  if ((msg < 0) || (msg >= m_msg_len)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Print the appropriate message data in a format based on the status
   * byte */
  if (((status >= 0x80) && (status <= 0xbf)) ||
      ((status >= 0xe0) && (status <= 0xef))) {
    /* Two data bytes -- make sure enough bytes remain */
    if (msg > m_msg_len - 2) {
      raiseErr(__LINE__, NULL);
    }
    
    /* Write the data bytes */
    writeBinary(pOut, &(m_msg[msg]), 2);
    
  } else if ((status >= 0xc0) && (status <= 0xdf)) {
    /* One data byte -- write the byte */
    writeByte(pOut, (int) m_msg[msg]);
    
  } else if (status == 0xf0) {
    /* Blob where first byte is implicit 0xF0 -- decode the handle
     * index */
    decodeVInt(&(m_msg[msg]), &h, m_msg_len - msg);
    
    /* Make sure handle exists and it is to a non-empty blob where the
     * first byte is 0xF0 */
    if ((h < 0) || (h >= m_h_len)) {
      raiseErr(__LINE__, NULL);
    }
    if (!((m_h[h]).is_blob)) {
      raiseErr(__LINE__, NULL);
    }
    if (blob_len((m_h[h]).ptr.pBlob) < 1) {
      raiseErr(__LINE__, NULL);
    }
    if ((blob_ptr((m_h[h]).ptr.pBlob))[0] != 0xf0) {
      raiseErr(__LINE__, NULL);
    }
    
    /* Write one less than the length of the blob as a variable-length
     * integer */
    printVInt(pOut, blob_len((m_h[h]).ptr.pBlob) - 1);
    
    /* If blob length more than one, write everything in the blob after
     * the first byte */
    if (blob_len((m_h[h]).ptr.pBlob) > 1) {
      writeBinary(
        pOut,
        &((blob_ptr((m_h[h]).ptr.pBlob))[1]),
        blob_len((m_h[h]).ptr.pBlob) - 1);
    }
    
  } else if (status == 0xf7) {
    /* Blob without any implicit bytes -- decode the handle index */
    decodeVInt(&(m_msg[msg]), &h, m_msg_len - msg);
    
    /* Make sure the handle exists and it is a blob */
    if ((h < 0) || (h >= m_h_len)) {
      raiseErr(__LINE__, NULL);
    }
    if (!((m_h[h]).is_blob)) {
      raiseErr(__LINE__, NULL);
    }
    
    /* Write the length of the blob as a variable-length integer */
    printVInt(pOut, blob_len((m_h[h]).ptr.pBlob));
    
    /* If blob is not empty, write everything in the blob */
    if (blob_len((m_h[h]).ptr.pBlob) > 0) {
      writeBinary(
        pOut,
        &((blob_ptr((m_h[h]).ptr.pBlob))[0]),
        blob_len((m_h[h]).ptr.pBlob));
    }
    
  } else if (status == 0xff) {
    /* Meta-event -- begin by getting the type code with the flag
     * indicating the payload format */
    ty = (int) m_msg[msg];
    
    /* Process depending on payload format */
    if ((ty & 0x80) != 0) {
      /* Indirect format, so first clear the flag on the type code and
       * then write the type code */
      ty = ty & 0x7f;
      writeByte(pOut, ty);
      
      /* Decode the handle index */
      decodeVInt(&(m_msg[msg + 1]), &h, m_msg_len - msg - 1);
      
      /* Make sure the handle exists */
      if ((h < 0) || (h >= m_h_len)) {
        raiseErr(__LINE__, NULL);
      }
      
      /* Print the blob or text length followed by the data */
      if ((m_h[h]).is_blob) {
        printVInt(pOut, blob_len((m_h[h]).ptr.pBlob));
        if (blob_len((m_h[h]).ptr.pBlob) > 0) {
          writeBinary(
            pOut,
            &((blob_ptr((m_h[h]).ptr.pBlob))[0]),
            blob_len((m_h[h]).ptr.pBlob));
        }
        
      } else {
        printVInt(pOut, text_len((m_h[h]).ptr.pText));
        if (text_len((m_h[h]).ptr.pText) > 0) {
          writeString(pOut, text_ptr((m_h[h]).ptr.pText));
        }
      }
      
    } else {
      /* Direct format, so first write the type code */
      writeByte(pOut, ty);
      
      /* Decode the data length and get the length of this length
       * declaration */
      cl = (int32_t) decodeVInt(
                        &(m_msg[msg + 1]),
                        &len,
                        m_msg_len - msg - 1);
      
      /* Make sure the type code, the data length declaration, and the
       * data all fit within the message buffer */
      if (len > m_msg_len - msg - cl - 1) {
        raiseErr(__LINE__, NULL);
      }
      
      /* Write the data length and the data */
      writeBinary(pOut, &(m_msg[msg + 1]), cl + len);
    }
    
  } else {
    raiseErr(__LINE__, NULL);
  }
}

/*
 * Compute the full size in bytes of a MIDI message, not including the
 * delta time.
 * 
 * sel is the selector of the MIDI message to size.  prev is the
 * selector of the previous MIDI message, or zero if no previous
 * message.  prev is only used for determining whether there is a
 * running status byte.
 * 
 * Parameters:
 * 
 *   sel - selector of MIDI message
 * 
 *   prev - selector of previous MIDI message, or zero if none
 * 
 * Return:
 * 
 *   the encoded size in bytes of this MIDI message, accounting for
 *   running status bytes, but not including any delta time
 */
static int32_t sizeMsg(uint32_t sel, uint32_t prev) {
  
  int32_t result = 0;
  int status = 0;
  int prev_status = 0;
  int ty = 0;
  int32_t msg = 0;
  int32_t h = 0;
  int32_t len = 0;
  int32_t cl = 0;
  
  /* Parse selector into status and message buffer offset */
  status = (int) (sel >> 24);
  msg = (int32_t) (sel & UINT32_C(0xffffff));
  
  /* Get previous status byte, zero if none */
  prev_status = (int) (prev >> 24);
  
  /* Increment size by one to account for the status byte */
  result++;
  
  /* If previous status byte is in range 0x80 to 0xEF inclusive, and it
   * matches the current status byte, then decrement size by one since
   * we won't write status byte due to running status */
  if ((prev_status >= 0x80) && (prev_status <= 0xef) &&
      (prev_status == status)) {
    result--;
  }
  
  /* Check that message offset is in range of message buffer */
  if ((msg < 0) || (msg >= m_msg_len)) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Compute the remaining size of the MIDI message */
  if (((status >= 0x80) && (status <= 0xbf)) ||
      ((status >= 0xe0) && (status <= 0xef))) {
    /* Two data bytes */
    result += 2;
    
  } else if ((status >= 0xc0) && (status <= 0xdf)) {
    /* One data byte */
    result++;
    
  } else if (status == 0xf0) {
    /* Blob where first byte is implicit 0xF0 -- decode the handle
     * index */
    decodeVInt(&(m_msg[msg]), &h, m_msg_len - msg);
    
    /* Make sure handle exists and it is to a non-empty blob where the
     * first byte is 0xF0 */
    if ((h < 0) || (h >= m_h_len)) {
      raiseErr(__LINE__, NULL);
    }
    if (!((m_h[h]).is_blob)) {
      raiseErr(__LINE__, NULL);
    }
    if (blob_len((m_h[h]).ptr.pBlob) < 1) {
      raiseErr(__LINE__, NULL);
    }
    if ((blob_ptr((m_h[h]).ptr.pBlob))[0] != 0xf0) {
      raiseErr(__LINE__, NULL);
    }
    
    /* Add the size of the length declaration and the size of the blob
     * data */
    result += sizeVInt(blob_len((m_h[h]).ptr.pBlob) - 1);
    result += blob_len((m_h[h]).ptr.pBlob) - 1;
    
  } else if (status == 0xf7) {
    /* Blob without any implicit bytes -- decode the handle index */
    decodeVInt(&(m_msg[msg]), &h, m_msg_len - msg);
    
    /* Make sure the handle exists and it is a blob */
    if ((h < 0) || (h >= m_h_len)) {
      raiseErr(__LINE__, NULL);
    }
    if (!((m_h[h]).is_blob)) {
      raiseErr(__LINE__, NULL);
    }
    
    /* Add the size of the length declaration and the size of the blob
     * data */
    result += sizeVInt(blob_len((m_h[h]).ptr.pBlob));
    result += blob_len((m_h[h]).ptr.pBlob);
    
  } else if (status == 0xff) {
    /* Meta-event -- begin by getting the type code with the flag
     * indicating the payload format */
    ty = (int) m_msg[msg];
    
    /* Add a byte in length for type code */
    result++;
    
    /* Process depending on payload format */
    if ((ty & 0x80) != 0) {
      /* Indirect format, so decode the handle index */
      decodeVInt(&(m_msg[msg + 1]), &h, m_msg_len - msg - 1);
      
      /* Make sure the handle exists */
      if ((h < 0) || (h >= m_h_len)) {
        raiseErr(__LINE__, NULL);
      }
      
      /* Add space for length declaration and the data */
      if ((m_h[h]).is_blob) {
        result += sizeVInt(blob_len((m_h[h]).ptr.pBlob));
        result += blob_len((m_h[h]).ptr.pBlob);
        
      } else {
        result += sizeVInt(text_len((m_h[h]).ptr.pText));
        result += text_len((m_h[h]).ptr.pText);
      }
      
    } else {
      /* Direct format, so decode the data length and get the length of
       * this length declaration */
      cl = (int32_t) decodeVInt(
                        &(m_msg[msg + 1]),
                        &len,
                        m_msg_len - msg - 1);
      
      /* Make sure the type code, the data length declaration, and the
       * data all fit within the message buffer */
      if (len > m_msg_len - msg - cl - 1) {
        raiseErr(__LINE__, NULL);
      }
      
      /* Add space for the data length and the data */
      result += cl + len;
    }
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  /* Return encoded size */
  return result;
}

/*
 * Make room in capacity for a given number of elements in the header
 * table.
 * 
 * n is the number of additional elements beyond current length to make
 * room for.  It must be zero or greater.
 * 
 * Upon return, the capacity will be at least n elements higher than the
 * current length.
 * 
 * An error occurs if the requested expansion would go beyond the
 * maximum allowed capacity.
 * 
 * Parameters:
 * 
 *   n - the number of elements to make room for
 */
static void capHead(int32_t n) {
  
  int32_t target = 0;
  int32_t new_cap = 0;
  
  /* Check parameters */
  if (n < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Only proceed if n is non-zero */
  if (n > 0) {
    
    /* Make initial allocation if necessary */
    if (m_head_cap < 1) {
      m_head = (uint32_t *) calloc(
                (size_t) HEAD_INIT_CAP, sizeof(uint32_t));
      if (m_head == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      m_head_cap = HEAD_INIT_CAP;
      m_head_len = 0;
    }
    
    /* Compute target length */
    if (n <= INT32_MAX - m_head_len) {
      target = m_head_len + n;
    } else {
      raiseErr(__LINE__, "MIDI header table capacity exceeded");
    }
    
    /* Only proceed if target length exceeds current capacity */
    if (target > m_head_cap) {
      /* Check that target within maximum capacity */
      if (target > HEAD_MAX_CAP) {
        raiseErr(__LINE__, "MIDI header table capacity exceeded");
      }
      
      /* Compute new capacity by doubling current capacity until greater
       * than or equal to target length, and then limiting to maximum
       * capacity */
      new_cap = m_head_cap;
      while (new_cap < target) {
        new_cap *= 2;
      }
      if (new_cap > HEAD_MAX_CAP) {
        new_cap = HEAD_MAX_CAP;
      }
      
      /* Expand capacity */
      m_head = (uint32_t *) realloc(m_head,
                            ((size_t) new_cap) * sizeof(uint32_t));
      if (m_head == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      memset(
        &(m_head[m_head_cap]),
        0,
        ((size_t) (new_cap - m_head_cap)) * sizeof(uint32_t));
      
      m_head_cap = new_cap;
    }
  }
}

/*
 * Add a MIDI message to the header buffer.
 * 
 * Use one of the addMsg functions first to get a MIDI message selector,
 * and then add that selector to the header with this function.
 * 
 * The order in which events are added to the header buffer will match
 * the order in which they are output to the start of the MIDI track.
 * 
 * Parameters:
 * 
 *   sel - the selector of the MIDI message
 */
static void addHeadMsg(uint32_t sel) {
  capHead(1);
  m_head[m_head_len] = sel;
  m_head_len++;
}

/*
 * Make room in capacity for a given number of elements in the moment
 * table.
 * 
 * n is the number of additional elements beyond current length to make
 * room for.  It must be zero or greater.
 * 
 * Upon return, the capacity will be at least n elements higher than the
 * current length.
 * 
 * An error occurs if the requested expansion would go beyond the
 * maximum allowed capacity.
 * 
 * Parameters:
 * 
 *   n - the number of elements to make room for
 */
static void capMoment(int32_t n) {
  
  int32_t target = 0;
  int32_t new_cap = 0;
  
  /* Check parameters */
  if (n < 0) {
    raiseErr(__LINE__, NULL);
  }
  
  /* Only proceed if n is non-zero */
  if (n > 0) {
    
    /* Make initial allocation if necessary */
    if (m_moment_cap < 1) {
      m_moment = (MOMENT *) calloc(
                (size_t) MOMENT_INIT_CAP, sizeof(MOMENT));
      if (m_moment == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      m_moment_cap = MOMENT_INIT_CAP;
      m_moment_len = 0;
    }
    
    /* Compute target length */
    if (n <= INT32_MAX - m_moment_len) {
      target = m_moment_len + n;
    } else {
      raiseErr(__LINE__, "MIDI moment table capacity exceeded");
    }
    
    /* Only proceed if target length exceeds current capacity */
    if (target > m_moment_cap) {
      /* Check that target within maximum capacity */
      if (target > MOMENT_MAX_CAP) {
        raiseErr(__LINE__, "MIDI moment table capacity exceeded");
      }
      
      /* Compute new capacity by doubling current capacity until greater
       * than or equal to target length, and then limiting to maximum
       * capacity */
      new_cap = m_moment_cap;
      while (new_cap < target) {
        new_cap *= 2;
      }
      if (new_cap > MOMENT_MAX_CAP) {
        new_cap = MOMENT_MAX_CAP;
      }
      
      /* Expand capacity */
      m_moment = (MOMENT *) realloc(m_moment,
                            ((size_t) new_cap) * sizeof(MOMENT));
      if (m_moment == NULL) {
        raiseErr(__LINE__, "Out of memory");
      }
      
      memset(
        &(m_moment[m_moment_cap]),
        0,
        ((size_t) (new_cap - m_moment_cap)) * sizeof(MOMENT));
      
      m_moment_cap = new_cap;
    }
  }
}

/*
 * Add a MIDI message to the moment buffer.
 * 
 * Use one of the addMsg functions first to get a MIDI message selector,
 * and then add that selector to the header with this function.
 * 
 * t is the moment offset of the event.  The event range will be updated
 * by this function.  Messages in the moment buffer will be output after
 * the events in the header buffer.
 * 
 * This function will automatically assign a unique event ID to the
 * message using eventID().
 * 
 * Parameters:
 * 
 *   t - moment offset
 * 
 *   sel - the selector of the MIDI message
 */
static void addMomentMsg(int32_t t, uint32_t sel) {
  capMoment(1);
  (m_moment[m_moment_len]).eid = eventID();
  (m_moment[m_moment_len]).t   = t;
  (m_moment[m_moment_len]).sel = sel;
  m_moment_len++;
  eventRange(t);
}

/*
 * Comparison function for sorting the moment buffer.
 * 
 * The interface of this function matches the standard library qsort()
 * callback function.
 * 
 * Both of the elements should be MOMENT structures.
 * 
 * The first comparison is by moment offsets.  Further comparisons are
 * only used if both moment offsets are identical.
 * 
 * The second comparison is by status class from the status byte in the
 * message selectors.  Status bytes in range 0x80 to 0xAF inclusive are
 * class 2 and all other status bytes are class 1.  Class 1 sorts before
 * class 2.  Further comparisons are only used if both classes are the
 * same.
 * 
 * The third comparison is by specific status byte.  However, status
 * bytes in range 0xF0 to 0xFF inclusive are treated as if they are the
 * same.  Further comparisons are only used if both status bytes are
 * the same or both are in range 0xF0 to 0xFF inclusive.
 * 
 * The fourth and final comparison is by event ID.  All moment
 * structures are supposed to have unique event IDs, so this should
 * resolve all remaining ambiguities.
 * 
 * Parameters:
 * 
 *   pA - the first structure to compare
 * 
 *   pB - the second structure to compare
 * 
 * Return:
 * 
 *   less than zero, equal to zero, or greater than zero as the first
 *   element is less than, equal to, or greater than the second
 */
static int cmpMoment(const void *pA, const void *pB) {
  
  int result = 0;
  const MOMENT *e1 = NULL;
  const MOMENT *e2 = NULL;
  int s1 = 0;
  int s2 = 0;
  int c1 = 0;
  int c2 = 0;
  
  if ((pA == NULL) || (pB == NULL)) {
    raiseErr(__LINE__, NULL);
  }
  
  e1 = (const MOMENT *) pA;
  e2 = (const MOMENT *) pB;
  
  if (e1->t > e2->t) {
    result = 1;
  } else if (e1->t < e2->t) {
    result = -1;
  }
  
  if (result == 0) {
    s1 = (int) (e1->sel >> 24);
    s2 = (int) (e2->sel >> 24);
    
    if ((s1 >= 0x80) && (s1 <= 0xaf)) {
      c1 = 2;
    } else {
      c1 = 1;
    }
    
    if ((s2 >= 0x80) && (s2 <= 0xaf)) {
      c2 = 2;
    } else {
      c2 = 1;
    }
    
    if (c1 > c2) {
      result = 1;
    } else if (c1 < c2) {
      result = -1;
    }
  }
  
  if (result == 0) {
    if ((s1 >= 0xf0) && (s1 <= 0xff)) {
      s1 = 0xf0;
    }
    if ((s2 >= 0xf0) && (s2 <= 0xff)) {
      s2 = 0xf0;
    }
    
    if (s1 > s2) {
      result = 1;
    } else if (s1 < s2) {
      result = -1;
    }
  }
  
  if (result == 0) {
    if (e1->eid > e2->eid) {
      result = 1;
    } else if (e1->eid < e2->eid) {
      result = -1;
    }
  }
  
  return result;
}

/*
 * Public function implementations
 * ===============================
 * 
 * See the header for specifications.
 */

/*
 * midi_null function.
 */
void midi_null(int32_t t, int head) {
  if (m_compiled) {
    raiseErr(__LINE__, "MIDI module already compiled");
  }
  if (!head) {
    eventRange(t);
  }
}

/*
 * midi_text function.
 */
void midi_text(int32_t t, int head, int tclass, TEXT *pText) {
  
  uint32_t sel = 0;
  
  if (m_compiled) {
    raiseErr(__LINE__, "MIDI module already compiled");
  }
  if ((tclass < MIDI_TEXT_MIN_VAL) || (tclass > MIDI_TEXT_MAX_VAL)) {
    raiseErr(__LINE__, NULL);
  }
  if (pText == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  sel = addMsgMT(0xff, tclass, pText);
  
  if (head) {
    addHeadMsg(sel);
  } else {
    addMomentMsg(t, sel);
  }
}

/*
 * midi_tempo function.
 */
void midi_tempo(int32_t t, int head, int32_t val) {
  
  uint32_t sel = 0;
  uint8_t buf[3];
  
  memset(buf, 0, 3);
  
  if (m_compiled) {
    raiseErr(__LINE__, "MIDI module already compiled");
  }
  if ((val < MIDI_TEMPO_MIN) || (val > MIDI_TEMPO_MAX)) {
    raiseErr(__LINE__, NULL);
  }

  buf[0] = (uint8_t) ((val >> 16) & 0xff);
  buf[1] = (uint8_t) ((val >>  8) & 0xff);
  buf[2] = (uint8_t) ( val        & 0xff);

  sel = addMsgMD(0xff, 0x51, buf, 3);
  
  if (head) {
    addHeadMsg(sel);
  } else {
    addMomentMsg(t, sel);
  }
}

/*
 * midi_time_sig function.
 */
void midi_time_sig(int32_t t, int head, int num, int denom, int metro) {
  
  uint32_t sel = 0;
  int i = 0;
  int j = 0;
  uint8_t buf[4];
  
  memset(buf, 0, 4);
  
  if (m_compiled) {
    raiseErr(__LINE__, "MIDI module already compiled");
  }
  if ((num < 1) || (num > MIDI_TIME_NUM_MAX) ||
      (denom < 1) || (denom > MIDI_TIME_DENOM_MAX)) {
    raiseErr(__LINE__, NULL);
  }
  if ((metro < 1) || (metro > MIDI_TIME_METRO_MAX)) {
    raiseErr(__LINE__, NULL);
  }
  
  j = 0;
  for(i = denom; i > 1; i = i / 2) {
    if ((i % 2) != 0) {
      raiseErr(__LINE__, NULL);
    }
    j++;
  }
  
  buf[0] = (uint8_t) num;
  buf[1] = (uint8_t) j;
  buf[2] = (uint8_t) metro;
  buf[3] = (uint8_t) 8;
  
  sel = addMsgMD(0xff, 0x58, buf, 4);
  
  if (head) {
    addHeadMsg(sel);
  } else {
    addMomentMsg(t, sel);
  }
}

/*
 * midi_key_sig function.
 */
void midi_key_sig(int32_t t, int head, int count, int minor) {
  
  uint32_t sel = 0;
  uint8_t buf[2];
  
  memset(buf, 0, 2);
  
  if (m_compiled) {
    raiseErr(__LINE__, "MIDI module already compiled");
  }
  if ((count < MIDI_KEY_COUNT_MIN) || (count > MIDI_KEY_COUNT_MAX)) {
    raiseErr(__LINE__, NULL);
  }
  
  if (count < 0) {
    count = count + 256;
  }
  
  buf[0] = (uint8_t) count;
  if (minor) {
    buf[1] = (uint8_t) 1;
  } else {
    buf[1] = (uint8_t) 0;
  }
  
  sel = addMsgMD(0xff, 0x59, buf, 2);
  
  if (head) {
    addHeadMsg(sel);
  } else {
    addMomentMsg(t, sel);
  }
}

/*
 * midi_custom function.
 */
void midi_custom(int32_t t, int head, BLOB *pData) {
  
  uint32_t sel = 0;
  
  if (m_compiled) {
    raiseErr(__LINE__, "MIDI module already compiled");
  }
  if (pData == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  sel = addMsgMB(0xff, 0x7f, pData);
  
  if (head) {
    addHeadMsg(sel);
  } else {
    addMomentMsg(t, sel);
  }
}

/*
 * midi_system function.
 */
void midi_system(int32_t t, int head, BLOB *pData) {
  
  uint32_t sel = 0;
  int stype = 0;
  
  if (m_compiled) {
    raiseErr(__LINE__, "MIDI module already compiled");
  }
  if (pData == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  stype = 0xf7;
  if (blob_len(pData) > 0) {
    if ((blob_ptr(pData))[0] == 0xf0) {
      stype = 0xf0;
    }
  }
  
  sel = addMsgB(stype, pData);
  
  if (head) {
    addHeadMsg(sel);
  } else {
    addMomentMsg(t, sel);
  }
}

/*
 * midi_message function.
 */
void midi_message(
    int32_t t,
    int     head,
    int     ch,
    int     msg,
    int     idx,
    int     val) {
  
  uint32_t sel = 0;
  int status = 0;
  int a = 0;
  int b = 0;
  
  if (m_compiled) {
    raiseErr(__LINE__, "MIDI module already compiled");
  }
  if ((ch < 1) || (ch > MIDI_CH_MAX)) {
    raiseErr(__LINE__, NULL);
  }
  
  status = (msg << 4) | (ch - 1);
  
  if ((msg == MIDI_MSG_NOTE_OFF       ) ||
      (msg == MIDI_MSG_NOTE_ON        ) ||
      (msg == MIDI_MSG_POLY_AFTERTOUCH) ||
      (msg == MIDI_MSG_CONTROL        )) {
    if ((idx < 0) || (idx > MIDI_DATA_MAX)) {
      raiseErr(__LINE__, NULL);
    }
    if ((val < 0) || (val > MIDI_DATA_MAX)) {
      raiseErr(__LINE__, NULL);
    }
    
    sel = addMsg2(status, idx, val);
    
  } else if ((msg == MIDI_MSG_PROGRAM) ||
              (msg == MIDI_MSG_CH_AFTERTOUCH)) {
    if ((val < 0) || (val > MIDI_DATA_MAX)) {
      raiseErr(__LINE__, NULL);
    }
    
    sel = addMsg1(status, val);
    
  } else if (msg == MIDI_MSG_PITCH_BEND) {
    if ((val < 0) || (val > MIDI_WIDE_MAX)) {
      raiseErr(__LINE__, NULL);
    }
    
    a = (int) (val & 0x7f);
    b = (int) ((val >> 7) & 0x7f);
    
    sel = addMsg2(status, a, b);
    
  } else {
    raiseErr(__LINE__, NULL);
  }
  
  if (head) {
    addHeadMsg(sel);
  } else {
    addMomentMsg(t, sel);
  }
}

/*
 * midi_range_lower function.
 */
int32_t midi_range_lower(void) {
  if (m_compiled) {
    raiseErr(__LINE__, "MIDI module already compiled");
  }
  return m_lower;
}

/*
 * midi_range_upper function.
 */
int32_t midi_range_upper(void) {
  if (m_compiled) {
    raiseErr(__LINE__, "MIDI module already compiled");
  }
  return m_upper;
}

/*
 * midi_compile function.
 */
void midi_compile(FILE *pOut) {
  
  uint32_t sel = 0;
  uint32_t prev_sel = 0;
  int32_t i = 0;
  int32_t len = 0;
  int32_t mlen = 0;
  int32_t dlen = 0;
  
  /* Check state and set compilation flag */
  if (m_compiled) {
    raiseErr(__LINE__, "MIDI module already compiled");
  }
  m_compiled = 1;
  
  /* Check parameters */
  if (pOut == NULL) {
    raiseErr(__LINE__, NULL);
  }
  
  /* If at least two records in moment buffer, sort the moment buffer */
  if (m_moment_len > 1) {
    qsort(m_moment, (size_t) m_moment_len, sizeof(MOMENT), &cmpMoment);
  }
  
  /* Cap the moment buffer by appending End Of Track message, using
   * upper bound of event range with end-of-moment moment part */
  sel = addMsgMD(0xff, 0x2f, NULL, 0);
  addMomentMsg(pointer_pack(m_upper, 2), sel);
  
  /* Convert moment offsets within moment structures into subquantum
   * offsets from the lower bound of event range, and then convert to
   * delta time from previous event */
  for(i = 0; i < m_moment_len; i++) {
    (m_moment[i]).t = pointer_unpack((m_moment[i]).t, NULL) - m_lower;
    if (i > 0) {
      (m_moment[i]).t = (m_moment[i]).t - (m_moment[i - 1]).t;
    }
  }
  
  /* Compute the length of the MIDI track by first computing the encoded
   * length of all messages in the header buffer, along with one byte
   * each for the delta codes of zero */
  for(i = 0; i < m_head_len; i++) {
    sel = m_head[i];
    mlen = sizeMsg(sel, prev_sel);
    dlen = 1;
    
    if (len <= INT32_MAX - mlen - dlen) {
      len += mlen + dlen;
    } else {
      raiseErr(__LINE__, "Compiled MIDI track too large");
    }
    
    prev_sel = sel;
  }
  
  /* Finish length computation by computing the encoded length of all
   * messages in the moment buffer, along with the encoded length of
   * their delta values; also check that delta times are all within
   * range */
  for(i = 0; i < m_moment_len; i++) {
    sel = (m_moment[i]).sel;
    mlen = sizeMsg(sel, prev_sel);
    
    if (((m_moment[i]).t < 0) ||
        ((m_moment[i]).t > INT32_C(0x0FFFFFFF))) {
      raiseErr(__LINE__, "MIDI delta time overflow");
    }
    
    dlen = sizeVInt((m_moment[i]).t);
    
    if (len <= INT32_MAX - mlen - dlen) {
      len += mlen + dlen;
    } else {
      raiseErr(__LINE__, "Compiled MIDI track too large");
    }
    
    prev_sel = sel;
  }
  
  /* Now write the MIDI headers and open the track chunk so that we are
   * ready to start writing delta times and MIDI events */
  writeString(pOut, "MThd");
  writeUint32BE(pOut, (uint32_t) 6);    /* Length of head chunk */
  writeUint16BE(pOut, 0);               /* Format 0 */
  writeUint16BE(pOut, (uint16_t) 1);    /* One track */
  writeUint16BE(pOut, (uint16_t) 768);  /* Delta units per quarter */
  
  writeString(pOut, "MTrk");
  writeUint32BE(pOut, (uint32_t) len);  /* Length of MIDI track */
  
  /* Write all the MIDI messages in the header buffer, each with a delta
   * time of zero */
  for(i = 0; i < m_head_len; i++) {
    printVInt(pOut, 0);
    printMsg(pOut, m_head[i]);
  }
  
  /* Write all the MIDI messages in the moment buffer, each with the
   * encoded delta time */
  for(i = 0; i < m_moment_len; i++) {
    printVInt(pOut, (m_moment[i]).t);
    printMsg(pOut, (m_moment[i]).sel);
  }
  
  /* Shut down the MIDI module */
  if (m_h != NULL) {
    free(m_h);
    m_h = NULL;
  }
  
  if (m_msg != NULL) {
    free(m_msg);
    m_msg = NULL;
  }
  
  if (m_head != NULL) {
    free(m_head);
    m_head = NULL;
  }
  
  if (m_moment != NULL) {
    free(m_moment);
    m_moment = NULL;
  }
  
  m_h_cap = 0;
  m_h_len = 0;
  
  m_msg_cap = 0;
  m_msg_len = 0;
  
  m_head_cap = 0;
  m_head_len = 0;
  
  m_moment_cap = 0;
  m_moment_len = 0;
}
