# Infrared articulation map

An _articulation map_ tells Infrared how to interpret the articulations and grace notes that it encounters within an NMF file.  Articulations determine both the velocity of note-on events, as well as the actual performance duration of notes.

See the documentation file on timing systems for the details of how event timing works in Infrared.  The _subquanta_ timing unit referred to within this document is defined in the timing system documentation, as is the distinction between _notated duration_ and _performance duration._  Furthermore, the transformations described in the articulation table take place in the intermediate timing format, prior to applying the factoring algorithm.

## Articulations

The articulation map defines _articulations._  Each articulation defines how the notated durations transform into performance durations, and it also defines the MIDI key-down velocity that is used for the note.

Articulations both transform the durations and define the key-down velocities of all non-grace notes.  For grace notes, articulations only define the key-down velocities and do not affect the durations.  Grace note offsets and durations are defined separately by _grace timings,_ which are described later.

The MIDI key-down velocity of an articulation is simply given as an integer value in range 1 to 127 (inclusive).  This velocity is applied to all grace and non-grace notes that the articulation applies to.

The duration transformation is specified as three integers:

1. Multiplier
2. Bumper
3. Gap

The _multiplier_ is always an integer in range 1 to 8.  The first step in duration transformation is to multiply the duration in subquanta by the multiplier and then divide by 8.  This scales the performance duration to a fraction of the notated duration, in units of 1/8 of subquantum.  Since subquanta were derived by multiplying all the original quanta by eight, dividing by eight will always yield an integer result.

The _bumper_ is always an integer greater than zero.  After the multiplier is applied to the duration, if the result is less than the bumper, the duration is lengthened to the bumper duration.

The _gap_ is always an integer greater than zero.  After the multiplier and bumper are applied, if the result is greater than the notated duration subtracted by the gap, the duration is reduced to the notated duration subtracted by the gap.  (If the notated duration is less than or equal to the gap, the final transformed duration is one.)

In short, the multiplier scales the notated duration, then the bumper imposes a minimum duration, and the gap imposes a maximum duration relative to the notated duration.

The factoring algorithm described in the document about timing systems will work best if the scaling factor established by the multiplier can be described as a rational number with a denominator as small as possible.  For example, a multiplier of 8 will work best with the factoring algorithm because 8/8 reduces to 1/1.  A multiplier of 4 is next best because 4/8 reduces to 1/2.  Multipliers of 2 and 6 are next because 2/8 and 6/8 reduce to 1/4 and 3/4.  Finally, multipliers of 1, 3, 5, and 7 are least effective with the factoring algorithm because the denominator of their scaling factor can't be reduced from 8.

For the bumper and gap values, values that are divisible by a higher power of two will generally work better with the factoring algorithm than others.  However, choosing values so high that all the durations end up clipped to length one will not work well.

## Grace timings

The articulation map also defines _grace timings._  Grace timings determine the actual performance time offset and duration of the grace notes they are applied to.

The grace timing is specified with two integers:

1. Slot
2. Gap

The _slot_ defines the total duration of the time slot the grace note occupies.  The first grace note before the beat will occupy the first slot before the beat.  The second grace note before the beat will occupy the second slot before the beat, and so forth.  The slot must be an integer that is two or greater, and it is measured in subquanta.

The _gap_ defines the amount of silence at the end of each slot.  The gap must be at least one and also less than the slot duration.

Suppose there is a grace note before a beat.  The beat occurs at time offset _b_ (measured in subquanta) and the grace note index is _i_ which is an integer less than zero where -1 means the first grace note before the beat, -2 means the second grace note before the beat, and so forth.  If a grace timing with slot _s_ and gap _g_ is applied to this grace note, the grace note will be positioned at:

    g_offset = b + (i * s)
    g_dur    = s - g

An error occurs if the computed grace note offset is less than zero.  If you have grace notes near the beginning of the performance that would violate this restriction, Infrared allows a fixed offset to be applied to every event, delaying the start of the performance.  This then allows grace notes that would otherwise be before time offset zero to occur at a time offset that is zero or greater.

The factoring algorithm will generally work best if the slot and gap are each divisible by as high a power of two as is possible.

## Application bitmaps

When an event is encountered, Infrared needs to determine which articulation applies to it, and -- if it is a grace note -- which grace timing applies to it.  This mapping is determined by _application bitmaps._

Each articulation record and each grace timing record has an application bitmap, which consists of exactly four base-16 digits.  These four base-16 digits represent a total of 16 bits, with each of these 16 bits representing one of the 16 MIDI channels.  The most significant bit of the first digit is the first MIDI channel and the least significant bit of the last digit is the last MIDI channel.

When a bit is set in the application bitmap, the record applies to the corresponding MIDI channel.  When a bit is clear in the application bitmap, the record does not apply to the corresponding MIDI channel.  The application bitmap allows a record to apply to any possible combination of MIDI channels.  For example, the bitmap `FFFF` applies to all 16 MIDI channels, while the bitmap `6000` applies to the second and third MIDI channels (because the second and third most significant bits of the first digit are set).

## Matrices

Infrared determines which articulation definition applies to which note using _matrices._

An Infrared matrix covers every possible pairing of MIDI channel and NMF articulation.  Since there are 16 MIDI channels and 62 NMF articulations, there are 992 possible pairings of channel and articulation in the matrix.  Each of these pairings is mapped to a specific articulation definition.

There are two instances of the matrix that Infrared uses.  The _primary matrix_ is used for non-grace notes.  The _secondary matrix_ is used for grace notes.

Upon initialization, all cells in the primary and secondary matrices are initialized with the same default articulation definition.  This definition has a key-down velocity of 64 (considered by the MIDI standard to be the default or neutral velocity), a multiplier of 8 (decodes to a scaling value of 1.0 which means no scaling), and a bumper and gap both equal to 8 (each equivalent to a single NMF quantum).

Articulation records are processed in the order they appear in the articulation map file, and this order is significant!  An articulation record identifies a specific articulation, has an application bitmap that indicates which of the MIDI channels it applies to, and has a matrix selector that determines whether it modifies the primary, secondary, or both matrices.

All the selected cells are overwritten with the new articulation defined by the articulation record, while everything else is left intact.  With a matrix selector choosing both matrices and specifying `FFFF` as the application bitmap, a single articulation record will modify 32 matrix cells simultaneously (16 in the primary matrix and 16 in the secondary).

## Bank

Infrared also has a _bank_ of 16 grace timing definitions.  This is used to determine the grace timing that is used on each MIDI channel.  Each grace timing record updates a set of cells in this bank as determined by the application bitmap.  As with articulation records, the order of grace timing records is significant, because later records partially overwrite the results of earlier records.

Upon initialization, all cells in the bank are initialized with the same default grace timing.  This default grace timing has a slot of 48 (matches a 64th note, the smallest rhythmic unit in Noir) and a gap of 8 (equivalent to a single NMF quantum).

## Text format

The articulation map is a plain-text ASCII format.  Line breaks may be either LF or CR+LF.  Each line is trimmed of trailing spaces and tabs, but not of leading spaces and tabs.  No UTF-8 Byte Order Mark (BOM) is allowed to appear at the start of the file.

The first line in an articulation map must be a header line.  This must be a _case-sensitive_ match for the following:

    IR-ArtMap

All lines following the header line must either be blank, comment, articulation record, or grace record lines.  Blank lines are empty or contain nothing other than tabs and spaces.  Comment lines begin with an ASCII apostrophe (which may _not_ be preceded by whitespace) and the rest of the line is ignored and may contain anything, including non-ASCII characters.

The two types of records lines are described below.  The order of record lines is significant!

## Articulation record lines

Articulation record lines have the following format:

    k:00f5 4/8 1:2 120

The first character is the articulation, which uses the same articulation keys that Noir does.  There are 62 articulations total, with the decimal digits, then the uppercase ASCII letters, and finally the lowercase ASCII letters.  _This character is case sensitive!_

The second character is the matrix selector, which is either `:` or `!` or `$`.  The colon `:` means the record selects cells from both the primary and secondary matrices.  The exclamation mark `!` means the record selects cells from the secondary matrix only (grace notes).  The dollar sign `$` means the record selects cells from the primary matrix only (non-grace notes).

After the matrix selector comes the application bitmap, which is always exactly four base-16 digits (case-insensitive).  This selects the MIDI channels the record applies to, as explained in earlier sections.

The `4/8` specifies the scaling value.  The denominator must be 1, 2, 4, or 8.  The numerator must be greater than zero and less than or equal to the denominator.

The `1:2` specifies the bumper and gap, respectively.  Both of these are in subquanta units.  Both must be greater than zero.

The `120` is a decimal value indicating the MIDI velocity to use.  It must be in range 1 to 127, inclusive.

## Grace record lines

Grace record lines are a special kind of record line that have the following format:

    !00f5 8 -2

The first character for a grace record is always `!`.  This is followed by an application bitmap of exactly four base-16 digits, which selects which cells in the bank to modify.

The `8` is the slot duration in subquanta.  It must be at least 2.

The `-2` is the gap, measured in subquanta and expressed as a negative number.  This must always be less than zero and when added to the slot duration must result in a value greater than zero.
