# Infrared timing systems

Infrared uses two timing systems to schedule musical events:

1. Input timing system
2. Output timing system

The _input timing system_ is the timing system used in the Noir Music Format (NMF) input files.

The _output timing system_ is the timing system used in the MIDI output files that Infrared generates.

Each timing system is described in detail in the following sections.

## Input timing system

NMF files contain note events.  Note events are scheduled according to the input timing system using the following parameters:

1. Reference offset
2. Weight
3. Articulation

The _reference offset_ is the notated time offset of the note event.  For all measured notes, the reference offset is the time at which the note starts.  However, for unmeasured grace notes, the reference offset is the beat the grace note is attached to, and the actual start of the grace note occurs sometime before the reference offset.

Reference offsets are integer values zero or greater where zero is the beginning of the composition.  The time units are _quanta._  NMF files allow for different types of quantum definitions, but Infrared only supports the definition used for output from the `noir` notation program, which defines a quantum as 1/96 of a quarter note.

The _weight_ is the type of rhythmic notation used for the note event.  This can be a positive integer greater than zero, zero, or a negative integer.

When the weight is a positive integer greater than zero, it represents a measured rhyhthmic value in quantum units.  For example, a weight of 96 represents a quarter note and a weight of 120 represents a quarter note tied to a sixteenth note.

When the weight is zero, the event represents some non-note event such as a cue.  The `noir` notation program does not generate events with weights of zero, and Infrared will ignore all such events.

When the weight is a negative integer, it represents an unmeasured grace note.  -1 means the unmeasured grace note immediately before the beat, -2 means the unmeasured grace note immediately before that, and so forth.  The reference offset in this case defines which beat the grace note occurs before.

The _articulation_ influences how the weight is transformed into a performance duration.  For example, an articulation representing legato might cause the performance duration of a quarter-note weight to be a full quarter-note duration, while an articulation representing staccato might cause the performance duration of a quarter-note weight to be only an eighth-note duration.

## Output timing system

MIDI files schedule events according to integer _delta time units._  MIDI files support two kinds of delta time unit definitions.  The first kind defines delta time units as integer subdivisions of a quarter note.  The second kind defines delta time units as integer subdivisions of an SMPTE frame.

Infrared only supports an output timing system that defines delta time units as subdivisions of a quarter note, which is the most common timing system for MIDI files and also best matches the input timing system.

Within the header of MIDI files, there is an integer declaration of how many delta time units there are within a single quarter note.  MIDI systems synchronize with Timing Clock messages that are sent at a rate of 24 Timing Clock messages per quarter note.  Therefore, the declared rate of delta time units per quarter note is usually a multiple of 24, so that there is an integer number of delta time units per Timing Clock message, though this is not required by the MIDI file standard.

The declared rate of delta time units per quarter note has a range of 1 to 32767 (inclusive).  96 delta time units per quarter note is a common setting, but various multiples of 24 that are less than 1000 are also commonly used.  Infrared always uses a division of 768 delta time units per quarter note.  A single delta time unit at this rate is referred to by Infrared as a _subquantum_ because a single NMF quantum converts into eight subquanta, which allows for fine-grained conversion between the notated timings used in the NMF file and the performance timings used in MIDI.

The duration of a quarter note is controlled by the _tempo._  The tempo is specified in MIDI files as the duration in microseconds of the quarter note.  The theoretical range of tempi is 1 microsecond per quarter note up to 16,777,215 microseconds per quarter note, which corresponds to a range of approximately 3.576 quarters per minute up to 60,000,000 quarters per minute.  However, few MIDI systems actually support 60,000,000 quarters per minute in practice.

The standard MIDI hardware interface requires 320 microseconds to send a byte of data.  There are 24 Timing Clock messages per quarter note, and each Timing Clock message is one byte.  Therefore, if the MIDI hardware interface does nothing but send Timing Clock messages, the minimum possible duration of a quarter note is 7680 microseconds, which corresponds to 7812.5 quarters per minute.  If there are to be other MIDI events besides Timing Clock messages, the tempo should be significantly less than this practical maximum of 7812.5 quarters per minute.

The default tempo is 500,000 microseconds per quarter, which corresponds to 120 quarters per minute.  The tempo can be changed at any time using the Set Tempo meta-event within a MIDI file track.  Therefore, the duration of a delta time unit within a MIDI file is not constant, and instead varies depending on the current tempo setting.

Note that the MIDI concept of tempo differs from the usual musical concept of tempo because MIDI tempo is always specified in quarter notes regardless of the time signature, while musical tempo is based on a beat that is not always a quarter note.
