# Infrared timing systems

## Input timing system

Infrared input files are in the Noir Music Format (NMF).  NMF files time events according to _quanta._  Quanta are always integer values.  NMF allows for multiple kinds of quanta definitions, but Infrared only supports the quanta definition of 96 quanta per quarter note, which is also the quanta definition generated by the `noir` program.

Therefore, the NMF input to Infrared is always timed according to integer units of quanta, and there are always exactly 96 quanta in a quarter note.

## Output timing system

Infrared generates output MIDI files.  MIDI files time events according to _delta time units._  MIDI files support two kinds of delta time unit definitions.  The first kind defines delta time units as integer subdivisions of a _MIDI beat._  The second kind defines delta time units as integer subdivisions of an _SMPTE frame._

Infrared only supports generating output MIDI files that define delta time units as subdivisions of a MIDI beat, which is the most common timing system for MIDI files.

(Beats are often referred to as "MIDI quarter notes" within MIDI documentation, though MIDI quarter notes do not necessarily match notated quarter notes.)

Within the header of MIDI files, there is an integer declaration of how many delta time units there are within a single MIDI beat.  MIDI systems synchronize with Timing Clock messages that are sent at a rate of 24 Timing Clock messages per MIDI beat.  Therefore, the declared rate of delta time units per MIDI beat is usually a multiple of 24, so that there is an integer number of delta time units per Timing Clock message, though this is not required by the MIDI file standard.

The declared rate of delta time units per MIDI beat has a range of 1 to 32767 (inclusive).  96 delta time units per MIDI beat is a common setting, but various multiples of 24 that are less than 1000 are also commonly used.

The duration of a MIDI beat is controlled by the _tempo._  The tempo is specified in MIDI files as the duration in microseconds of the MIDI beat.  The theoretical range of tempi is 1 microsecond per beat up to 16,777,215 microseconds per beat, which corresponds to a range of approximately 3.576 beats per minute up to 60,000,000 beats per minute.  However, few MIDI systems actually support 60,000,000 beats per minute in practice.

The standard MIDI hardware interface requires 320 microseconds to send a byte of data.  There are 24 Timing Clock messages per MIDI beat, and each Timing Clock message is one byte.  Therefore, if the MIDI hardware interface does nothing but send Timing Clock messages, the minimum possible duration of a MIDI beat is 7680 microseconds, which corresponds to 7812.5 beats per minute.  If there are to be other MIDI events besides Timing Clock messages, the tempo should be significantly less than this practical maximum of 7812.5 beats per minute.

The default tempo is 500,000 microseconds per beat, which corresponds to 120 beats per minute.  The tempo can be changed at any time using the Set Tempo meta-event within a MIDI file track.  Therefore, the duration of a delta time unit within a MIDI file is not constant, and instead varies depending on the current tempo setting.

## Intermediate timing system

Infrared uses its own timing system to transform the input NMF timing system into the output MIDI timing system.  This is the _intermediate timing system._

Although the input NMF timing appears similar to the output MIDI timing, there is an important distinction.  In the input NMF timing, the duration of a note corresponds to the notated rhythmic value.  In the output MIDI timing, the duration of an event corresponds to how long it generates sound during the performance.  We can therefore contrast the _notated duration_ in the input NMF to the _performance duration_ in the output MIDI.

The notated duration and performance duration of an event may be significantly different.  For example, in staccato articulation, a long notated duration may only have a short performance duration so that there are gaps of silence between individual notes.

The input NMF timing system also allows for grace notes preceding a beat, while the output MIDI timing system has no concept of grace notes.

In order to convert between these two notions of duration, Infrared's intermediate timing system begins by converting NMF input quanta into intermediate system _subquanta._  There are exactly 8 subquanta in a quantum.  Therefore, while the input NMF has 96 quanta per quarter note, the intermediate timing system has 768 subquanta per quarter note.

After the timing offsets and durations have been converted from quanta into subquanta, the durations of notes are adjusted to convert them from notated duration to performance duration according to the articulation table.  Furthermore, grace notes are transformed into regular notes with a performance time offset and performance duration specified in subquanta.  See the articulation table documentation for further details of this adjustment.

Once all events have been adjusted to performance duration in the subquanta time scale, it is possible to directly output the timing to MIDI delta time by declaring a rate of 768 delta time units per quarter note.  However, Infrared supports _factoring,_ which simplifies time units as much as possible and can reduce the delta time unit rate.  The factoring alogrithm is described in the following subsection.

### Factoring algorithm

Events are timed in the intermediate timing system according to a rate of 768 subquanta per quarter note.  The _factoring algorithm_ attempts to reduce this rate without changing any of the timing.

768 is decomposed entirely into the product of prime numbers 2 and 3 as follows:

    768 = 2^8 x 3

For each non-zero event offset and event duration in subquanta, determine how many times it is divisible by 2 and how many times it is divisible by 3.  If the value is divisible by 2 more than 8 times, we only need to know it is so divisible eight times.  If the value is divisible by 3 more than one time, we only need to know it is so divisible one time.

More formally, given an integer _v_ greater than zero, let _M(v)_ be the minimum of 8 and the largest integer _r_ such that _v_ is divisible by `2^r` and let _N(v)_ be the minimum of 1 and the largest integer _s_ such that _v_ is divisible by `3^s`.

Now, let _m_ be the minimum of _M(v)_ for each non-zero event offset and event duration _v_ in the performance.  Also, let _n_ be the minimum of _N(v)_ for each non-zero event offset and event duration _v_ in the performance.  If there are no non-zero event offsets or durations in the performance, let _m_ be 8 and _n_ be 1.

We can then define a rate _w_ as follows:

           768
    w = ---------
        2^m * 3^n

_w_ will always be an integer in range 1 to 768.  This represents the minimum rate of _factored quanta_ per quarter note, such that each factored quantum has exactly `(768 / w)` subquanta, which will always be an integer value equal to `(2^m * 3^n)`.

The generated MIDI file then uses the factored quanta rate as the number of delta time units per quarter note, and adjusts all timing by converting subquanta into factored quanta.