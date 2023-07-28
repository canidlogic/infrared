# Infrared script specification

Infrared transforms an input file in Noir Music Format (NMF) into an output file in MIDI (.MID) format.  The details of this transformation are controlled by an Infrared script.  This document specifies the format of the Infrared script.

## Data types

Infrared scripts support a few different data types which are described in the subsections below.

Some of the data types are _primitive_ while others are _objects._  Primitive data types are stored directly within the interpreter memory cells and copied around.  Object data types are stored in separate tables and the interpreter memory cells then reference records within those separate tables.

Infrared has a simple memory management strategy for objects.  Each allocated object has a lifetime that proceeds all the way to the end of interpretation.  No attempt is made to garbage-collect unreachable objects before then.

### Integer

The _integer_ data type is a signed, 32-bit integer.  It has a range of -2,147,483,647 to 2,147,483,647 (inclusive).  Note that the two's-complement least negative value of -2,147,483,648 is _not_ in this integer range.  This is a primitive data type.

### Range

The _range_ data type is a bit string of exactly 64 bits.  The first 62 bits each correspond to one of the 62 NMF articulations.  The last 2 bits correspond to the primary (measured, non-grace note) and secondary (unmeasured grace note) articulation matrices.  Bits that are set select that articulation or matrix while bits that are clear mask out that articulation or matrix.  This is a primitive data type.

### Bitmap

The _bitmap_ data type is a bit string of exactly 16 bits.  Each bit corresponds to one of the 16 MIDI channels.  Bits that are set select that MIDI channel while bits that are clear mask out that MIDI channel.  This is a primitive data type.

### Text

The _text_ data type is an ASCII string of text.  Only visible, printing ASCII characters and the ASCII space are allowed (range 0x20 to 0x7E inclusive).  The length of a text object must be in range zero to 1023 characters.  Text objects are immutable after definition.

### Blob

The _blob_ data type is a binary string.  It may include any byte values including zero and including byte values in range 0x80 to 0x7F.  The length of a blob must be in range zero to 1,048,576 bytes.  Blob objects are immutable after definition.

### Articulation

The _articulation_ data type is an object with four fields:

1. Velocity
2. Scale
3. Bumper
4. Gap

The _velocity_ is the MIDI note-on velocity that will be applied to all notes transformed by this articulation.  It is an integer that must be in range 1 to 127 inclusive.  64 is considered by the MIDI standard to be the default "neutral" velocity.

The scale, bumper, and gap are only applied to measured (non-grace) notes transformed by this articulation.  They are ignored when transforming unmeasured grace notes.

The _scale_ is a rational number that has a denominator of either 1, 2, 4, or 8 and a numerator that is an integer greater than zero and less than or equal to the denominator.  Rational numbers of equal value are equivalent, such that 1/2, 2/4, and 4/8 have exactly the same meaning.

The _bumper_ is an integer greater than zero.  The bumper is measured in _subquantum_ units, where there are exactly eight subquanta in a single NMF quantum.  (See the separate document on timing systems for further information.)

The _gap_ is an integer less than or equal to zero.  The gap is also measured in subquantum units.

A measured (non-grace) note from the NMF file has its notated duration transformed into the MIDI performance duration using the following method:

First, the notated duration is translated into subquantum units by multiplying the NMF duration by 8.

Second, the subquantum duration is multiplied by the scale.  (Dividing by the denominator of the scale will always yield an integer result because we multiplied each NMF duration by 8.)

Third, if the duration is less than the bumper, the duration is extended to the bumper duration.

Fourth, if the duration is greater than the notated duration reduced by the gap, the duration is shortened to the notated duration reduced by the gap.

Fifth, if the resulting duration is less than one subquantum unit, the resulting duration is set to one subquantum unit.

Articulation objects are immutable after definition.

### Ruler

The _ruler_ data type is an object with two fields:

1. Slot
2. Gap

Rulers define how unmeasured grace notes are timed during a MIDI performance.  The input NMF file allows unmeasured grace notes to be specified by their offset before a beat, with -1 meaning the grace note immediately before the beat, -2 meaning the grace note immediately before that, and so forth.  The output MIDI file has no concept of unmeasured grace notes, so each NMF grace note must be transformed into a measured note using a ruler.

The _slot_ defines the total duration of the time slot unmeasured grace notes occupy.  The first grace note before the beat will occupy the first slot before the beat.  The second grace note before the beat will occupy the second slot before the beat, and so forth.  The slot must be an integer greater than zero, and it is measured in subquanta.

The _gap_ defines the amount of silence at the end of each slot, similar to the gap used in articulations.  The gap must be an integer number of subquanta less than or equal to zero.  Furthermore, when the slot duration is reduced by the gap, the result must be greater than zero.

Suppose there is a grace note before a beat.  The beat occurs at time offset _b_ (measured in subquanta) and the grace note index is _i_ which is an integer less than zero where -1 means the first grace note before the beat, -2 means the second grace note before the beat, and so forth.  If a grace timing with slot _s_ and gap _g_ is applied to this grace note, the grace note will be positioned at:

    g_offset = b + (i * s)
    g_dur    = s + g

In the Infrared sequence, it is acceptable for the computed grace note offset to be a negative time offset.  However, output MIDI files do not allow events with negative time offsets.  The Infrared sequence is converted to a MIDI sequence by applying window adjustments, described in a later section.  If you have grace notes near the beginning of the performance that would end up with a negative time offset in the MIDI file, you should see the window-before offset to prevent negative time offsets in the MIDI file.

Ruler objects are immutable after definition.

### Pointer

The _pointer_ data type represents a logical time position within the performance, which is used to position MIDI events defined within the Infrared script.

The pointer has a special value that makes it a _header pointer._  The header pointer means that events should be added to the header buffer rather than the moment buffer (see later for definitions).  This means that events timed with a header pointer will always have a time offset of zero in the output MIDI file regardless of any window settings.

When the time pointer is not the header pointer, it is defined by the following fields:

1. Section number
2. Quantum offset
3. Grace pickup
4. Ruler
5. Tilt
6. Moment part

The _section number_ selects one of the sections defined in the NMF file.

The _quantum offset_ is a time offset relative to the start of the selected NMF section, measured in NMF quanta (96 quanta per quarter note).  The offset may be greater than zero, zero, or negative.  If it is negative, it may select time offsets before the beginning time of the section.

The _grace pickup_ is an integer that is zero or negative.  If zero, it selects the time offset specified by the section number and quantum offset.  If negative, it uses the time offset specified by the section number and quantum offset as the beat, and then the negative value is an unmeasured grace note before that beat, where -1 is the grace note immediately before the beat, -2 is the grace note immediately before that, and so forth.

The _ruler_ is a reference to a ruler object that determines how to convert unmeasured grace note indices into time offsets.  It is only specified if the grace pickup is less than zero.  Otherwise, it is an undefined reference.

The _tilt_ is an integer offset that can be zero, greater than zero, or less than zero.  It is an offset in subquanta (1/8 of an NMF quantum) that is added to the offset after computing a time offset from the section number, quantum offset, grace pickup, and ruler.

The _moment part_ is either zero, one, or two, which selects either the start-of-moment, middle-of-moment, or end-of-moment position within a moment in the moment buffer.  See the later section on the moment buffer for further information.

A non-header pointer can be translated into a moment offset (see the later section on the moment buffer) by the following procedure:

First, look up the quantum offset of the section from the NMF file, with an error if the indicated section number is not defined.

Second, add the quantum offset within the pointer to the result of the previous step.

Third, multiply the result of the previous step by 8 to convert it into a subquantum offset.

Fourth, if the grace pickup is not zero, use the grace pickup index with the ruler object to get the offset of the indicated unmeasured grace note, as explained in the section about rulers.  If the grace pickup is zero, ignore this step.

Fifth, add the tilt to the subquantum offset computed in previous steps.

Sixth, multiply the result of the previous step by 3 and add the moment part to get the moment offset.

The result of these steps might be a negative time offset, which is acceptable within the Infrared sequence.  Window adjustments (see later) will be made prior to output to MIDI, which can remove these negative values by adding a fixed offset to all events except those in the header buffer.

Pointer types are objects, and furthermore they are mutable.

## Interpreter state

Infrared scripts are run in the context of _interpreter state._  The interpreter state always starts out in the same initial state.  The Infrared script then makes modifications to this initial state.  At the end of the Infrared script, the final interpreter state is then the state that is used to transform the input NMF file into an output MIDI file.

### Articulation matrices

An _articulation matrix_ is a two-dimensional array where one dimension is an NMF articulation, the other dimension is a MIDI channel, and each array element is a reference to an articulation object.  There are 62 different NMF articulations and 16 MIDI channels, so an articulation matrix has 992 total elements.

Infrared interpreter state contains two articulation matrices:  a primary matrix and a secondary matrix.  The _primary matrix_ is used for measured notes, which are all notes except unmeasured grace notes.  The _secondary matrix_ is used for unmeasured grace notes.  Note that only the velocity field of articulation objects in the secondary matrix is significant.  The duration of unmeasured grace notes is determined by the ruler bank, not the secondary articulation matrix.

Each note in the input NMF file is associated with a specific articulation object by the following process.  First, select the secondary matrix if the note is an unmeasured grace note or the primary matrix if the note is not an unmeasured grace note.  Second, within the selected matrix, select an articulation object using the MIDI channel the note belongs to (determined by the layer in the NMF file) and the NMF articulation as indices into the two-dimensional array.

The initial state of both articulation matrices has every matrix element referencing the same _default articulation object,_ which is a predefined articulation object.  This default articulation object has a scale of 1/1, a bumper of 8 (equivalent to one NMF quantum), a gap of zero, and a velocity of 64 (the default or neutral MIDI velocity).

### Ruler bank

Infrared interpreter state includes a _ruler bank,_ which is an array of 16 ruler objects.  The ruler bank associates a ruler object with each of the 16 MIDI channels.

Each unmeasured grace note in the input NMF file is associated with a ruler bank by checking which MIDI channel the grace note belongs to (determined by the layer in the NMF file) and then selecting the corresponding ruler object from the ruler bank.

The initial state of the ruler bank has every element referencing the same _default ruler object,_ which is a predefined ruler object.  The default ruler object has a slot of 48 subquanta (matches an NMF 64th note, the smallest rhythmic unit in Noir) and a gap of zero.

### Window settings

The _window settings_ of the Infrared interpreter state determine how much extra time is inserted before the start of the performance and after the last event of the performance finishes.

Infrared compiles its own sequence of events, which might have negative offset, and which always ends when the last defined event completes.  However, MIDI files do not allow negative time offsets.  Also, synthesizers often produce sound for a while even after the completion of the last event due to envelope release, reverb, and so froth.  Therefore, it is often desirable to add some extra time at the end of the MIDI sequence.

The _window-before_ is a count of subquanta units, zero or greater, that is added to each Infrared sequence event time offset to convert it into a MIDI event time offset.  This can remove negative time offsets and also create a time gap at the start of the performance.  An error will occur if any events still have negative time offsets after applying the window-before offset.

However, certain events should always have a time offset of zero regardless of the window-before settings.  Infrared has a special header buffer for these events so that their zero offsets do not get modified by the window settings.  See the header and moment buffers section for further information.

The _window-after_ is a count of subquanta units, zero or greater, that is added to the performance after the last defined event (adjusted by the window-before) completes.  This is necessary because MIDI files explicitly mark the end of the performance with a timed End Of Track meta-event, while NMF input files do not have any explicitly defined end marker.  Synthesizers often continue generating sound for a while even after all notes are released, so it is a good idea to leave some extra time at the end of the performance for everything to complete.

### Header and moment buffers

The input NMF input files only specify the note events, while the window settings determine the End Of Track meta-event.  There are a number of other events that should be included in MIDI files, such as Set Tempo meta-events to set the tempo, text events to insert meta-data, program changes to select instrumentation, and so forth.  These additional events are stored in the header and moment buffers.

The _header buffer_ contains MIDI events that must occur at time offset zero before any note events.  Events in the header buffer are the only events that are _not_ affected by the window-before setting.  Examples of events that should be in the header buffer are the Set Tempo event that sets the initial tempo, text events that provide meta-data, and messages that initialize the MIDI synthesizer.

The _moment buffer_ contains MIDI events that should be inserted at the appropriate time within the stream of note events.  Events in the moment buffer _are_ affected by the window-before setting so that they are synchronized with the note events.  Furthermore, the end of the performance is determined by the last note event or the last event in the moment buffer, whichever completes last.  (The window-after is then added to this latest time offset to determine the actual End Of Track event.)

Events within the moment buffer use a special _moment offset_ to indicate their timing.  The moment offset is defined as follows:

    moment_offset = (subquanta_offset * 3) + m
    where:
      m = 0 for start of moment, or
      m = 1 for middle of moment, or
      m = 2 for end of moment

The "start of moment" means that the event should take place before any note events, including note events that turn off notes that are ending at this time offset.  Set Tempo events are an example of events that should be at the start of a moment.

The "middle of moment" means that the event should take place after all note events that turn off notes ending at the time offset, but before all note events that are starting new notes.  Most events should go in the middle of the moment.

The "end of moment" means that the event should take place after all note events, both those that turn off notes ending at the time offset and those that start new notes at the time event.

At a given subquanta offset, then, the order of events is as follows:

1. Start-of-moment events
2. Note-off(*) events
3. Middle-of-moment events
4. Note-on events
5. End-of-moment events

(*) - Note-off events are implemented as note-on events with velocity of zero, which is allowed by the MIDI specification.

Within a single moment, multiple start-of-moment, multiple middle-of-moment, and multiple end-of-moment events are ordered by the order in which they were defined in the Infrared script.

### Time pointer

The time pointer state is used for positioning where MIDI events will be inserted into the header or moment buffers.  The time pointer is only used to define the time offsets of MIDI events created by the Infrared script.  It does not affect note events defined by the input NMF file, nor does it affect the End Of Track meta-event.

The time pointer begins as the special _header pointer,_ which means that events will be added to the header buffer and receive a time offset of zero regardless of any window settings.

An operation is provided that allows the time pointer to be loaded with the value of a given pointer object.  The pointer object is converted in a header pointer or a moment offset using the procedure discussed in the definition of pointers.  The header pointer or moment offset is then stored in the time pointer.  Further changes to the given pointer object after it has been loaded do _not_ affect the time pointer, because the time pointer only has a copy of the computed moment offset or the special header selector.

The time pointer also has a hidden field that is not accessible to Infrared scripts.  This hidden field starts at a value of zero and increments after each MIDI event that is scheduled by the Infrared script.  Infrared's internal sequence will store both the computed header selector or moment offset, along with this hidden field value.  This allows MIDI events that are defined at the same time to be sorted by the order in which they were defined in the Infrared script.

## MIDI events

Infrared scripts only allow certain kinds of MIDI events to be defined and placed in the header or moment buffers.  The following subsections describe the kinds of MIDI events that are allowed.

Note that MIDI note messages and End Of Track will of course be present, but they are determined entirely by the input NMF file and the window settings and can not be manually inserted within the Infrared script.

### Tempo events

Tempo events are meta-events that indicate the duration of quarter notes in microseconds.  This controls the speed of the performance, and it can be changed at various times in the performance to effect tempo changes.

Infrared allows tempo events that directly specify the duration of a quarter note in microseconds.  However, this is not the usual way that tempo is defined musically.  Infrared therefore allows tempo to alternatively be specified as the number of NMF quanta in a single beat and then the number of beats per minute as a floating-point value.  Infrared will then automatically compute the equivalent duration of a quarter note in microseconds.

There should always be a tempo event in the header buffer that sets the initial tempo.  All tempo changes after that should be timed events in the moment buffer.  If no tempo is established at the start of the performance, the MIDI standard recommends a default initial tempo of 120 quarters per minute (or 500,000 microseconds per quarter).

### Time signature events

Time signature events are meta-events that add metrical meta-data to the MIDI file.  Time signature meta-events are optional and do not affect the actual MIDI performance.  The initial time signature should be set in the header buffer and time signature changes should be timed events in the moment buffer.

The time signature is specified as an integer numerator in range 1 to 255 (inclusive) and an integer denominator in range 1 to 65536 (inclusive).  The denominator must be a power of two.

The time signature must also specify the rate at which a metronome clicks relative to the current tempo.  This rate is specified as the number of Timing Clock messages per metronome click, where there are always 24 Timing Clock messages in a quarter note.  For example, if the metronome should click every dotted quarter note, the rate should be set to 36 Timing Clocks per metronome click.

Time signature meta-events also allow the definition of a MIDI quarter note to be changed in case the MIDI file was generated with the MIDI quarter note redefined as something else.  This is never the case for Infrared, so this mapping is always set to a MIDI quarter note being a quarter note.

### Key signature events

Key signature events are meta-events that add key signature meta-data to the MIDI file.  Key signature meta-events are optional and do not affect the actual MIDI performance.  The initial key signature should be set in the header buffer and key signature changes should be timed events in the moment buffer.

The key signature is specified as the number of flats, the number of sharps, or no accidentals, where the maximum number of sharps or flats is 7.  The key signature also indicates whether it is for a major key or a minor key.

### Text events

Text events are meta-events that add meta-data to the MIDI file.  There are also text events that add timed information, such as timed lyrics, rehearsal numbers, and cue points.

Generally, text events stand by themselves.  However, the MIDI file standard says that the Instrument Name text message can optionally use a MIDI Channel Prefix message to indicate which channel the text message applies to, though the standard also indicates this information can be included as text within the body of the text message.  Since the MIDI Channel Prefix is optional in this case with an alternative, it is an exceptional case, and it could potentially alter the meaning of other meta-events, Infrared does not support using the MIDI Channel Prefix with Instrument Name messages.

Most text message types should be placed in the header buffer so that they occur at time zero no matter what.  However, Lyric, Marker, and Cue Point text events are timed and should be placed in the moment buffer.

### Channel messages

Infrared scripts can insert MIDI channel messages that apply to whole channels rather than individual notes.  The following channel messages are supported:

1. Control Change
2. Program Change
3. Channel Pressure
4. Pitch Bend

Along with the note events defined by the input NMF file, this allows for the full set of MIDI channel messages, except for note-off velocities and polyphonic key pressure, which are both specific to individual note events and would need support in the NMF file.

Each of these channel messages must define the specific MIDI channel it applies to, along with the message-specific information noted below.

Control Change selects a controller in range 0 to 127 (inclusive) and sets it to a value in range 0 to 127 (inclusive).  The controller meanings are defined in the MIDI standard, and a few have special meanings of setting channel modes.

Infrared also supplies some operations that bundle common sequences of Control Change messages together.  One of these operations sets a double-byte controller value with two separate Control Change messages.  The other operations set Registered or Non-registered parameter numbers either by value or by increment or by decrement.

Program Change selects a patch number defining the instrument that plays.  It is set to a value in range 0 to 127 (inclusive).

Channel Pressure changes the velocity of all notes currently active in the selected channel (aftertouch).  The aftertouch value is in range 0 to 127 (inclusive).

Pitch Bend bends the pitch of all notes currently active in the selected channel.  The pitch bend value is in the range -8192 to 8191, where zero is neutral, negative values bend the pitch down, and values greater than zero bend the pitch up.  The wideness of the pitch bend is not specified, and can be changed on many synthesizers with Control Change messages.

### Extension messages

Infrared scripts can insert binary extension data messages both in the header buffer and anywhere in the moment buffer.  MIDI files support three kinds of binary extension messages:

1. System-Exclusive
2. System-Exclusive Escape
3. Sequencer-Specific Meta

Each of these messages has a blob data type as its payload.  In all three cases, the blob must not be empty.  For System-Exclusive, the blob must begin with the byte value 0xF0.  System-Exclusive Escape and Sequencer-Specific Meta need not begin with 0xF0.

Normal System-Exclusive messages should use the System-Exclusive data extension type, beginning with a 0xF0 data byte and ending with an 0xF7 data byte.

If a single System-Exclusive message needs to be split up into packets with time delays between the packets, the first packet should be a System-Exclusive data message beginning with 0xF0, and all subsequent packets should be System-Exclusive Escape data messages, with the very last packet ending with data byte 0xF7.

System-Exclusive Escape messages can also be used to force other MIDI messages into the file, such as realtime messages.

Sequencer-Specific Meta messages are meta-events that are intended for custom extensions to the sequencer.  See the Standard MIDI Files specification for further information.
