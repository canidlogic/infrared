#ifndef CONTROL_H_INCLUDED
#define CONTROL_H_INCLUDED

/*
 * control.h
 * =========
 * 
 * Control module of Infrared.
 * 
 * Requirements
 * ------------
 * 
 * Requires the following Infrared modules:
 * 
 *   - blob.c
 *   - diagnostic.c
 *   - graph.c
 *   - midi.c
 *   - pointer.c
 *   - text.c
 */

#include <stddef.h>
#include <stdint.h>

#include "blob.h"
#include "graph.h"
#include "pointer.h"
#include "text.h"

/*
 * Constants
 * =========
 */

/*
 * The different modal messages.
 * 
 * The numeric constant values here match the reserved controller number
 * assigned to message, except for LOCAL_ON, which shares the same
 * reserved controller number as LOCAL_OFF.
 * 
 * The MAX and MIN constants bound the range of acceptable values,
 * except for the LOCAL_ON message which is exceptional as noted above.
 */
#define CONTROL_MODE_SOUND_OFF  (120)
#define CONTROL_MODE_RESET      (121)
#define CONTROL_MODE_LOCAL_OFF  (122)
#define CONTROL_MODE_LOCAL_ON   (222)   /* N.B. */
#define CONTROL_MODE_NOTES_OFF  (123)
#define CONTROL_MODE_OMNI_OFF   (124)
#define CONTROL_MODE_OMNI_ON    (125)
#define CONTROL_MODE_MONO       (126)
#define CONTROL_MODE_POLY       (127)

#define CONTROL_MODE_MIN        (120)
#define CONTROL_MODE_MAX        (127)

/*
 * The different controller types for automatic messages.
 * 
 * The MAX and MIN constants bound the range of acceptable values.
 */
#define CONTROL_TYPE_TEMPO    (1)
#define CONTROL_TYPE_7BIT     (2)
#define CONTROL_TYPE_14BIT    (3)
#define CONTROL_TYPE_NONREG   (4)
#define CONTROL_TYPE_REG      (5)
#define CONTROL_TYPE_PRESSURE (6)
#define CONTROL_TYPE_PITCH    (7)

#define CONTROL_TYPE_MIN      (1)
#define CONTROL_TYPE_MAX      (7)

/*
 * The maximum value for 14-bit indices and data.
 */
#define CONTROL_MAX_14BIT (0x3fff)

/*
 * The index of the data entry controller.
 * 
 * This is the most significant byte index.  The least significant byte
 * is at an index 0x20 greater, same as for the 14-bit controllers.
 */
#define CONTROL_INDEX_DATA (0x06)

/*
 * The control index range for 14-bit controllers.
 * 
 * The index CONTROL_INDEX_DATA is excluded from this range.
 * 
 * These indices select the most significant byte.  The least
 * significant byte is at an index 0x20 greater.
 */
#define CONTROL_INDEX_14BIT_MIN (0x1)
#define CONTROL_INDEX_14BIT_MAX (0x1f)

/*
 * The control index ranges for 7-bit controllers.
 * 
 * There are two such ranges.
 */
#define CONTROL_INDEX_7BIT_1_MIN (0x40)
#define CONTROL_INDEX_7BIT_1_MAX (0x5f)
#define CONTROL_INDEX_7BIT_2_MIN (0x66)
#define CONTROL_INDEX_7BIT_2_MAX (0x77)

/*
 * Public functions
 * ================
 */

/*
 * Add a "null" event.
 * 
 * This is a wrapper around the corresponding function of the MIDI
 * module.  See that function for further information.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - the location where the event shall be added
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void control_null(POINTER *pp, long lnum);

/*
 * Add a text meta-event.
 * 
 * This is a wrapper around the corresponding function of the MIDI
 * module.  See that function for further information.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - the location where the event shall be added
 * 
 *   tclass - the text message class
 * 
 *   pText - the text
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void control_text(POINTER *pp, int tclass, TEXT *pText, long lnum);

/*
 * Add a Time Signature meta-event.
 * 
 * This is a wrapper around the corresponding function of the MIDI
 * module.  See that function for further information.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - the location where the event shall be added
 * 
 *   num - the time signature numerator
 * 
 *   denom - the time signature denominator
 * 
 *   metro - the metronome click frequency
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void control_time_sig(
    POINTER * pp,
    int32_t   num,
    int32_t   denom,
    int32_t   metro,
    long      lnum);

/*
 * Add a Key Signature meta-event.
 * 
 * This is a wrapper around the corresponding function of the MIDI
 * module.  See that function for further information.  However, this
 * function is stricter as it requires the minor parameter to be either
 * one or zero.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - the location where the event shall be added
 * 
 *   count - number of accidentals in key signature
 * 
 *   minor - one for minor key, zero for major key
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void control_key_sig(
    POINTER * pp,
    int32_t   count,
    int32_t   minor,
    long      lnum);

/*
 * Add a Sequencer-Specific meta-event.
 * 
 * This is a wrapper around the corresponding function of the MIDI
 * module.  See that function for further information.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - the location where the event shall be added
 * 
 *   pData - the data payload
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void control_custom(POINTER *pp, BLOB *pData, long lnum);

/*
 * Add a System-Exclusive event.
 * 
 * This is a wrapper around the corresponding function of the MIDI
 * module.  See that function for further information.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - the location where the event shall be added
 * 
 *   pData - the message
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void control_system(POINTER *pp, BLOB *pData, long lnum);

/*
 * Add an instrument change.
 * 
 * ch is the channel number to change the instrument on.  It must be in
 * range 1 to MIDI_CH_MAX inclusive.
 * 
 * bank is the optional bank number to select.  If present, has_bank
 * must be non-zero.  If not present, has_bank is zero and bank is
 * ignored.  If present, bank must be in range 1 to 16384 inclusive.
 * Note that it is one-indexed, so that the first bank is bank 1, not
 * bank 0.
 * 
 * program is the instrument program to select.  It must always be
 * present and in range 1 to 128 inclusive.
 * 
 * This is a wrapper around midi_message() from the MIDI module, using
 * program change and possibly control change messages.  If there is no
 * bank, then this function just generates a program change message.  If
 * there is a bank, then this function generates two control change
 * messages to set the bank followed by a program change message.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - the location where the instrument change shall be added
 * 
 *   ch - the MIDI channel to set the instrument on
 * 
 *   bank - the bank of the instrument, if has_bank is non-zero
 * 
 *   program - the program number of the instrument
 * 
 *   has_bank - non-zero if bank parameter is present, zero if not
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void control_instrument(
    POINTER * pp,
    int32_t   ch,
    int32_t   bank,
    int32_t   program,
    int       has_bank,
    long      lnum);

/*
 * Add a channel mode message.
 * 
 * ch is the channel number to set the mode on.  It must be in range 1
 * to MIDI_CH_MAX inclusive.  According to the MIDI standard, channel
 * mode messages will only be recognized on a channel established as the
 * "Basic Channel" of a MIDI instrument.  The basic channel might be
 * established by a hard-wired setting in a MIDI instrument, by panel
 * settings on a MIDI instrument, or by some kind of System Exclusive
 * message to the instrument.
 * 
 * mtype selects the kind of channel mode message to send.  It must be
 * one of the CONTROL_MODE constants.
 * 
 * count is only used with the CONTROL_MODE_MONO message and ignored for
 * all other messages.  For CONTROL_MODE_MONO, it must either be a MIDI
 * channel in range 1 to MIDI_CH_MAX inclusive, or it must be the
 * special value zero.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   pp - the location where the modal message shall be added
 * 
 *   ch - the MIDI channel to send the modal message to
 * 
 *   mtype - the type of modal message
 * 
 *   count - for CONTROL_MODE_MONO, the number of channels or zero;
 *   ignored for all other message types
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void control_modal(
    POINTER * pp,
    int32_t   ch,
    int       mtype,
    int32_t   count,
    long      lnum);

/*
 * Schedule automatic graph tracking for a specific controller.
 * 
 * Unlike the other functions of this module, this function does not
 * directly generate any MIDI events.  Instead, controllers are
 * associated with graph objects.  Then, the control_track() function is
 * used to generate all necessary controller messages to track changes
 * in the mapped graph throughout the event range in the MIDI module.
 * 
 * Each specific controller may have at most one graph associated with
 * it.  If a graph is assigned to a controller that already has a graph,
 * the new graph overwrites the previous graph.
 * 
 * ctype is the type of controller being controlled.  It must be one of
 * the CONTROL_TYPE constants.
 * 
 * ch is the channel on which the specific MIDI controller resides.
 * Each type of controller has separate instances for each MIDI channel,
 * except for CONTROL_TYPE_TEMPO, which is not specific to any channel,
 * and for which the ch parameter is ignored.
 * 
 * idx is the index of the controller.  This is ignored for all
 * controller types except 7BIT, 14BIT, NONREG, and REG.  For 7BIT, it
 * must be either in the range bounded by the CONTROL_INDEX_7BIT_1
 * constants (inclusive), or in the range bounded by the constants
 * CONTROL_INDEX_7BIT_2 (inclusive).  For 14BIT, it must be in the range
 * bounded by the CONTROL_INDEX_14BIT constants (inclusive), excluding
 * CONTROL_INDEX_DATA.  For NONREG and REG, it must be in range zero to
 * CONTROL_MAX_14BIT (inclusive).
 * 
 * pg is the graph that controls the value of the controller over time.
 * For TEMPO controllers, the valid graph value range is MIDI_TEMPO_MIN
 * to MIDI_TEMPO_MAX as defined by the MIDI module.  For 7BIT and
 * PRESSURE controllers, the valid graph value range is zero to
 * MIDI_DATA_MAX as defined by the MIDI module.  For 14BIT, NONREG, REG,
 * and PITCH, the range is zero to CONTROL_MAX_14BIT.  All of these
 * ranges are inclusive of boundaries.
 * 
 * The given lnum should be from the Shastina parser, and it is used for
 * error reports if necessary.
 * 
 * Parameters:
 * 
 *   ctype - the type of controller
 * 
 *   ch - the MIDI channel of the controller; ignored for TEMPO
 * 
 *   idx - the index of the controller; only used for 7BIT, 14BIT,
 *   NONREG, and REG controllers
 * 
 *   pg - the graph that the controller should automatically track
 * 
 *   lnum - the Shastina line number for diagnostic messages
 */
void control_auto(
    int       ctype,
    int32_t   ch,
    int32_t   idx,
    GRAPH   * pg,
    long      lnum);

/*
 * Generate all the controller messages necessary to automatically track
 * controllers.
 * 
 * The tracked controllers must have previously been registered with the
 * control_auto() function.  This function will use the current event
 * range returned by the MIDI module as the boundaries of the tracking
 * range, so be sure to use this function AFTER adding all the other
 * kinds of messages to the MIDI module.
 */ 
void control_track(void);

#endif
