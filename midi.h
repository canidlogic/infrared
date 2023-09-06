#ifndef MIDI_H_INCLUDED
#define MIDI_H_INCLUDED

/*
 * midi.h
 * ======
 * 
 * MIDI output module of Infrared.
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Infrared modules:
 * 
 *   - blob.c
 *   - diagnostic.c
 *   - pointer.c
 *   - text.c
 */

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>

#include "blob.h"
#include "text.h"

/*
 * Constants
 * =========
 */

/*
 * The different text meta-event types.
 * 
 * The constant values match the values in the MIDI file specification.
 * 
 * The special MAX_VAL and MIN_VAL values store the full range of
 * possible values.
 */
#define MIDI_TEXT_GENERAL     (1)
#define MIDI_TEXT_COPYRIGHT   (2)
#define MIDI_TEXT_TITLE       (3)
#define MIDI_TEXT_INSTRUMENT  (4)
#define MIDI_TEXT_LYRIC       (5)
#define MIDI_TEXT_MARKER      (6)
#define MIDI_TEXT_CUE         (7)

#define MIDI_TEXT_MIN_VAL     (1)
#define MIDI_TEXT_MAX_VAL     (7)

/*
 * The full range of valid tempo settings.
 * 
 * These are microseconds per quarter note settings.
 */
#define MIDI_TEMPO_MIN (1)
#define MIDI_TEMPO_MAX INT32_C(16777215)

/*
 * Maximum values for time signature events.
 */
#define MIDI_TIME_NUM_MAX   (255)
#define MIDI_TIME_DENOM_MAX (1024)
#define MIDI_TIME_METRO_MAX (255)

/*
 * The full range of valid key signature count values.
 */
#define MIDI_KEY_COUNT_MIN (-7)
#define MIDI_KEY_COUNT_MAX (7)

/*
 * The maximum MIDI channel, where the first MIDI channel is considered
 * channel 1.
 */
#define MIDI_CH_MAX (16)

/*
 * The maximum value of MIDI channel message data values and wide-data
 * values.  The minimum values are zero.
 */
#define MIDI_DATA_MAX (127)
#define MIDI_WIDE_MAX (0x3fff)

/*
 * Different kinds of MIDI channel messages.
 * 
 * Each constant value represents the high nybble of the corresponding
 * MIDI status byte shifted over by four bits.
 */
#define MIDI_MSG_NOTE_OFF         (0x8)
#define MIDI_MSG_NOTE_ON          (0x9)
#define MIDI_MSG_POLY_AFTERTOUCH  (0xa)
#define MIDI_MSG_CONTROL          (0xb)
#define MIDI_MSG_PROGRAM          (0xc)
#define MIDI_MSG_CH_AFTERTOUCH    (0xd)
#define MIDI_MSG_PITCH_BEND       (0xe)

/*
 * Public functions
 * ================
 */

/*
 * Add a "null" event.
 * 
 * If head is non-zero, the event is added to the header buffer and t is
 * ignored.  If head is zero, the event is added to the moment buffer
 * using the moment offset t.
 * 
 * Adding null events to the header buffer has no effect.  Adding null
 * events to the moment buffer will expand the event range if necessary
 * but will not add any MIDI message.
 * 
 * Parameters:
 * 
 *   t - the moment offset, if head is zero
 * 
 *   head - non-zero for header buffer, zero for moment buffer
 */
void midi_null(int32_t t, int head);

/*
 * Add a text meta-event.
 * 
 * If head is non-zero, the event is added to the header buffer and t is
 * ignored.  If head is zero, the event is added to the moment buffer
 * using the moment offset t.
 * 
 * The text message class tclass must be in range MIDI_TEXT_MIN_VAL to
 * MIDI_TEXT_MAX_VAL.
 * 
 * Parameters:
 * 
 *   t - the moment offset, if head is zero
 * 
 *   head - non-zero for header buffer, zero for moment buffer
 * 
 *   tclass - the text message class
 * 
 *   pText - the text
 */
void midi_text(int32_t t, int head, int tclass, TEXT *pText);

/*
 * Add a Set Tempo meta-event.
 * 
 * If head is non-zero, the event is added to the header buffer and t is
 * ignored.  If head is zero, the event is added to the moment buffer
 * using the moment offset t.
 * 
 * The tempo value must be in range MIDI_TEMPO_MIN to MIDI_TEMPO_MAX,
 * inclusive.  This specifies the number of microseconds per quarter
 * note.
 * 
 * Parameters:
 * 
 *   t - the moment offset, if head is zero
 * 
 *   head - non-zero for header buffer, zero for moment buffer
 * 
 *   val - the number of microseconds per quarter note
 */
void midi_tempo(int32_t t, int head, int32_t val);

/*
 * Add a Time Signature meta-event.
 * 
 * If head is non-zero, the event is added to the header buffer and t is
 * ignored.  If head is zero, the event is added to the moment buffer
 * using the moment offset t.
 * 
 * num and denom are the numerator and denominator of the time
 * signature.  Both must be at least one and at most MIDI_TIME_NUM_MAX
 * or MIDI_TIME_DENOM_MAX, respectively.  The denominator must also be a
 * power of two.
 * 
 * metro is the number of MIDI clocks in a metronome click, where a MIDI
 * clock is 1/24 of a quarter note.  It must be in range one up to
 * MIDI_TIME_METRO_MAX, inclusive.
 * 
 * Parameters:
 * 
 *   t - the moment offset, if head is zero
 * 
 *   head - non-zero for header buffer, zero for moment buffer
 * 
 *   num - the time signature numerator
 * 
 *   denom - the time signature denominator
 * 
 *   metro - the metronome click frequency
 */
void midi_time_sig(int32_t t, int head, int num, int denom, int metro);

/*
 * Add a Key Signature meta-event.
 * 
 * If head is non-zero, the event is added to the header buffer and t is
 * ignored.  If head is zero, the event is added to the moment buffer
 * using the moment offset t.
 * 
 * count specifies the number of flats or sharps in the key signature.
 * It must be an integer in MIDI_KEY_COUNT_MIN to MIDI_KEY_COUNT_MAX
 * range, inclusive.  Negative values count flats and positive values
 * count sharps.
 * 
 * minor is non-zero for a minor key, zero for a major key.
 * 
 * Parameters:
 * 
 *   t - the moment offset, if head is zero
 * 
 *   head - non-zero for header buffer, zero for moment buffer
 * 
 *   count - number of accidentals in key signature
 * 
 *   minor - non-zero for minor key, zero for major key
 */
void midi_key_sig(int32_t t, int head, int count, int minor);

/*
 * Add a Sequencer-Specific meta-event.
 * 
 * If head is non-zero, the event is added to the header buffer and t is
 * ignored.  If head is zero, the event is added to the moment buffer
 * using the moment offset t.
 * 
 * pData is the data payload, which includes everything after the length
 * declaration.
 * 
 * Parameters:
 * 
 *   t - the moment offset, if head is zero
 * 
 *   head - non-zero for header buffer, zero for moment buffer
 * 
 *   pData - the data payload
 */
void midi_custom(int32_t t, int head, BLOB *pData);

/*
 * Add a System-Exclusive message.
 * 
 * If head is non-zero, the event is added to the header buffer and t is
 * ignored.  If head is zero, the event is added to the moment buffer
 * using the moment offset t.
 * 
 * pData is the message.  The message does *not* include the length
 * declaration that is stored in the MIDI file representation of system
 * exclusive messages.
 * 
 * If the message is not empty and starts with the byte value 0xF0, then
 * the main System-Exclusive message type will be used, with the initial
 * 0xF0 implicit and not repeated in the data payload despite its
 * presence in the blob.  In all other cases the "escape"
 * System-Exclusive message type.
 * 
 * Parameters:
 * 
 *   t - the moment offset, if head is zero
 * 
 *   head - non-zero for header buffer, zero for moment buffer
 * 
 *   pData - the message
 */
void midi_system(int32_t t, int head, BLOB *pData);

/*
 * Add a MIDI channel message.
 * 
 * If head is non-zero, the event is added to the header buffer and t is
 * ignored.  If head is zero, the event is added to the moment buffer
 * using the moment offset t.
 * 
 * ch is the MIDI channel the message is sent to.  It must be in range 1
 * to MIDI_CH_MAX inclusive.  Note that this is one-indexed (first
 * channel is channel one, not zero)!
 * 
 * msg is the specific MIDI channel message.  It must be one of the
 * MIDI_MSG constants.
 * 
 * idx is used for NOTE_OFF, NOTE_ON, POLY_AFTERTOUCH, and CONTROL
 * messages, and ignored for other message types.  For NOTE_OFF,
 * NOTE_ON, and POLY_AFTERTOUCH, the index is the MIDI key number.  For
 * CONTROL, the index is the controller number.  The valid range of idx
 * is 0 to MIDI_DATA_MAX, inclusive.
 * 
 * val is used for all message types to indicate the value associated
 * with the event.  When idx means the key number, val means the
 * velocity.  When idx means the controller number, val means the value
 * to send to the controller.  For all other messages, val is the
 * parameter value for the message.
 * 
 * For all message types except PITCH_BEND, val has a valid range of 0
 * to MIDI_DATA_MAX, inclusive.  For PITCH_BEND, val has a valid range
 * of 0 to MIDI_WIDE_MAX, inclusive.
 * 
 * Parameters:
 * 
 *   t - the moment offset, if head is zero
 * 
 *   head - non-zero for header buffer, zero for moment buffer
 * 
 *   ch - the one-indexed MIDI channel
 * 
 *   msg - the MIDI message type
 * 
 *   idx - the index value, or ignored if not relevant
 * 
 *   val - the parmeter value
 */
void midi_message(
    int32_t t,
    int     head,
    int     ch,
    int     msg,
    int     idx,
    int     val);

/*
 * Determine the current lower bound of the event range.
 * 
 * This is the minimum subquantum (not moment!) offset of all events
 * that have been entered into the MIDI module so far, including null
 * events.  If no events have yet been entered, zero is returned.
 * 
 * Return:
 * 
 *   the minimum subquantum offset of the event range
 */
int32_t midi_range_lower(void);

/*
 * Determine the current upper bound of the event range.
 * 
 * This is the maximum subquantum (not moment!) offset of all events
 * that have been entered into the MIDI module so far, including null
 * events.  If no events have yet been entered, zero is returned.
 * 
 * Return:
 * 
 *   the maximum subquantum offset of the event range
 */
int32_t midi_range_upper(void);

/*
 * Compile all the messages that have been entered into the MIDI module
 * into a MIDI file and write it to the given output file.
 * 
 * Writing is entirely sequential, so you can pass stdout.
 * 
 * After this function is called, it may not be called again, nor may
 * any further messages be added to the MIDI module.
 * 
 * Parameters:
 * 
 *   pOut - the file to write the compile MIDI file to
 */
void midi_compile(FILE *pOut);

#endif
