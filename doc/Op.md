# Infrared operations

This document specifies all the operations that are available to Infrared scripts.

The syntax of operations in this document has the following format:

    [in_1:type] [in_2:type] ... [in_n:type]
    opname
    [out_1:type] [out_2:type] ... [out_n:type]

The arguments that come before the operation name are the input arguments that must be present on the interpreter stack when the operation is invoked.  `[in_n]` will be the argument on top of the stack during input.

Each operation pops all its input arguments, performs the operation, and pushes all its output arguments.  When the operation returns, `[out_n]` will be the output argument on top of the stack.

If there are no input arguments, a hyphen is used before the operation name.  If there are no output arguments, a hyphen is used after the operation name.

The type `any` means that any data type is acceptable.  Otherwise, the type name will be one of the Infrared data types specified by the script documentation.

## Basic operations

    [in:any] pop -

Pop a single value of any type off the top of the stack and discard it.

    [in:any] dup [in:any] [in:any]

Pop a single value of any type off the top of the stack and push two copies of it back onto the stack.

## Diagnostic operations

    [in:any] print -

Pop a single value of any type off the top of the stack and print a string representation of it to the script diagnostic console.

    - newline -

Print a line break to the script diagnostic console.

    - stop -

Stop interpreting the script and end interpretation on an error.

## Arithmetic operations

    [a:Integer] [b:Integer] add [c:Integer]
    [a:Integer] [b:Integer] sub [c:Integer]
    [a:Integer] [b:Integer] mul [c:Integer]
    [a:Integer] [b:Integer] div [c:Integer]

Perform binary arithmetic operations using `a` as the first argument to the operation and `b` as the second argument to the operation.  The result is then pushed onto the stack as `c`.  An error occurs if there is an overflow beyond the range of an integer.  An error also occurs if there is division by zero.

For the division operation, the division is performed in floating-point space and then floored to an integer by rounding down towards negative infinity.

    [a:Integer] neg [b:Integer]

Take an integer from the stack and push its inverse, such that adding `a` and `b` together results in the value of zero.  Positive values greater than zero become negative, negative values become positive, and zero is unchanged.

## String operations

    [e1:Blob|Text] [e2:Blob|Text] ... [e_n:Blob|Text]
    [n:Integer]
    concat
    [r:Blob|Text]

Concatenate an existing sequence of blobs or texts together.  There must be at least one input element, and each input element must be of the same type (either all blobs or all texts).  You can use the Shastina array facility to automatically count the input elements and push the `n` input element.  An error occurs if the concatenated result is too large.  It is possible to use multiple instances of the same blob or text object in the input elements.

    [in:Blob|Text] [i:Integer] [j:Integer]
    slice
    [r:Blob|Text]

Get a subrange of an existing blob or text object.  `i` is the index of the first character or first byte in the blob or text belonging to the subrange.  `j` is one greater than the index of the last character or last byte in the blob or text belonging to the subrange.  `i` must be greater than or equal to zero and less than or equal to `j`.  `j` must be less than or equal to the length of the blob or text.  If `i` and `j` are equal, an empty blob or text results.

## Simple constructor operations

    [numerator:Integer]
    [denominator:Integer]
    [bumper:Integer]
    [gap:Integer]
    art
    [r:Articulation]

Construct a new articulation object.  The denominator must be 1, 2, 4, or 8.  The numerator must be greater than zero and less than or equal to the denominator.  The bumper is zero or greater and measured in subquanta.  The gap is zero or negative and measured in subquanta.

Articulations transform notated durations of measured (non-grace) notes in the NMF input to performance durations used in the MIDI performance.  First, the measured duration, in subquanta, is multiplied by the numerator and divided by the denominator.  Second, the scaled subquanta is limited to a minimum value given by the bumper.  Third, the subquanta duration is limited to a maximum value given by the gap added to the notated duration.  Fourth, the subquanta duration is brought up to a one subquantum if it is zero or negative.

    [slot:Integer] [gap:Integer] ruler [r:Ruler]

Construct a new ruler object.  The slot must be greater than zero and measured in subquanta.  The gap must be zero or negative, measured in subquanta, and when added to the slot, the result must be greater than zero.

Rulers position unmeasured grace notes in the NMF input.  The `slot` refers to how much total time each unmeasured grace note position occupies before the beat.  The `gap` determines how much silence there is between subsequent grace notes.

    - ptr [p:Pointer]

Construct a new pointer object and push it onto the stack.  The pointer starts out as a header pointer.

    [p:Pointer] reset [p:Pointer]

Pop a pointer off the stack, reset it to a header pointer, and push it back on the stack.  For other kinds of pointer modifications, use the suffixed numeric operations documented in the script specification.

## Graph construction operations

    [v:Integer] gval [g:Graph]

Construct a graph that has the given integer value `v` across its entire domain.  The given value can be any value zero or greater.  The resulting graph object is equivalent to the graph that would be obtained by constructing a graph in the graph accumulator with a single constant region.

    - begin_graph -
    - end_graph [g:Graph]

Construct a new graph by using the graph accumulator.  The `begin_graph` is used to start a new graph definition.  The `end_graph` is used to take the current definition in the graph accumulator, construct a graph object with it, push that new graph on the stack, and clear the graph accumulator.  Graph definitions with the graph accumulator may _not_ be nested.

    [p:Pointer] [v:Integer] graph_const -

Add a constant region to the graph definition curently in the accumulator.  The time this region begins is determined by `p`, which may not be a header pointer.  The constant region has the value `v` throughout its domain, and `v` must be zero or greater.  Regions must be defined in chronological order.

    [p:Pointer]
    [a:Integer] [b:Integer]
    [s:Integer] graph_ramp -
    
    [p:Pointer]
    [a:Integer] [b:Integer]
    [s:Integer] graph_ramp_log -

Add a ramp region to the graph definition currently in the accumulator.  The time this region begins is determined by `p`, which may not be a header pointer.  The ramp region may not be the last region in the graph.  It ends when the next region starts.  The ramp starts at graph value `a` and proceeds to graph value `b`, which would arrive exactly at the start of the next region.  Graph values change at subquanta offsets that are divisible by `s`, which must be greater than zero.

The regular `graph_ramp` will perform linear interpolation between the two values.  The `graph_ramp_log` will perform logarithmic interpolation between the two values.  If the `a` and `b` values are identical, the operations are equivalent to `graph_const`.

    [p:Pointer]
    [g:Graph] [pSrc:Pointer]
    [numerator:Integer]
    [denominator:Integer]
    [c:Integer]
    [min:Integer]
    [max:Integer] graph_derive -

Add a derived region to the graph definition currently in the accumulator.  The time this region begins at in the new graph definition is determined by `p`, which may not be a header pointer.  Graph values are copied from the existing graph `g` starting at the time given by `pSrc`, which may not be a header pointer.

Values are copied over from the source graph to the new graph definition.  They are first transformed by multiplying by the numerator and dividing by the denominator, then adding `c`, and then clamping to the range from `min` to `max` (inclusive).  Numerator must be zero or greater and denominator must be greater than zero.  `min` must be zero or greater.  `max` must be greater than or equal to `min`, unless it has the special value -1, which means there is no maximum limit.

## Set construction operations

    - begin_set -
    - end_set [s:Set]

Construct a new set by using the set accumulator.  The `begin_set` is used to start a new set definition, which starts out as an empty set.  The `end_set` is used to take the current definition in the set accumulator, construct a set object with it, push that new set on the stack, and clear the set accumulator.  Set definitions may _not_ be nested.

    - all -
    - none -

Modify an open set definition in the set accumulator by either including all possible values in it (`all`) or nothing in it (`none`).

    - invert -

Invert an open set definition so that everything that is included is no longer included, and everything that was not included is now included.

    [a:Integer] [b:Integer] include -
    [a:Integer] [b:Integer] exclude -

Modify an open set definition so that the closed range from `a` to `b` inclusive is either fully included or fully excluded in the set.  `a` must be zero or greater and `b` must be greater than or equal to `b`.

    [a:Integer] include_from -
    [a:Integer] exclude_from -

Modify an open set definition so that the open range beginning at `a` and proceeding to positive infinity is either fully included or fully excluded in the set.  `a` must be zero or greater.

    [s:Set] union -

Modify an open set definition by taking the union with an existing set object.

    [s:Set] intersect -

Modify an open set definition by taking the intersection with an existing set object.

    [s:Set] except -

Modify an open set definition to exclude anything that is in an existing set object.

## Control operations

    [p:Pointer] null_event -

Add a "null" event at the given position.  Null events do not actually add any messages to the MIDI file.  However, they may expand the event range of the MIDI performance, adding silence at the start or end of the performance.

    [p:Pointer] [t:Text] text -
    [p:Pointer] [t:Text] text_copyright -
    [p:Pointer] [t:Text] text_title -
    [p:Pointer] [t:Text] text_instrument -
    [p:Pointer] [t:Text] text_lyric -
    [p:Pointer] [t:Text] text_marker -
    [p:Pointer] [t:Text] text_cue -

Add a text event with the given text at the given position.  The `text` operation adds general textual metadata.  All the other operations add specific kind of textual metadata.

    [p:Pointer]
    [numerator:Integer]
    [denominator:Integer]
    [metro:Integer]
    time_sig -

Add a time signature metadata event at the given position.  The numerator and denominator must be greater than zero.  The numerator may be at most 255.  The denominator must be a power of two and at most 1024.

The `metro` parameter indicates how often the metronome clicks.  It is specified in units of 1/24 of a quarter note.  It must be greater than zero and at most 255.

    [p:Pointer] [count:Integer] major_key -
    [p:Pointer] [count:Integer] minor_key -

Add a key signature metadata event for a major or minor key at the given position.  The count is for the number of accidentals, with negative values counting flats, positive values counting sharps, and zero meaning no accidentals in the key signature.  The valid range of counts is -7 to +7 inclusive.

    [p:Pointer] [b:Blob] custom -

Add a sequencer-specific meta-event at the given position.  The blob contains the binary data.

    [p:Pointer] [b:Blob] sysex -

Add a system-exclusive event at the given position.  The blob contains the binary data.

    [p:Pointer] [ch:Integer] [v:Integer] program -
    [p:Pointer] [ch:Integer] [b:Integer] [v:Integer] patch -

Add an instrument change at the given position on the given MIDI channel.  The `ch` channel must be in range 1 to 16 inclusive.  The `v` is the instrument program number, in range 1 to 128 inclusive.  The `b` is the instrument bank, in range 1 to 16384 inclusive.

    [p:Pointer] [ch:Integer] sound_off -
    [p:Pointer] [ch:Integer] midi_reset -
    [p:Pointer] [ch:Integer] local_off -
    [p:Pointer] [ch:Integer] local_on -
    [p:Pointer] [ch:Integer] notes_off -
    [p:Pointer] [ch:Integer] omni_off -
    [p:Pointer] [ch:Integer] omni_on -
    [p:Pointer] [ch:Integer] [count:Integer] mono -
    [p:Pointer] [ch:Integer] poly -

Add a modal channel message at the given position on the given MIDI channel.  The `ch` channel must be in range 1 to 16 inclusive.  The `mono` operation also requires a channel count, which must be in range 0 to 16, where zero is a special value.

    [g:Graph] auto_tempo -
    [ch:Integer] [idx:Integer] [g:Graph] auto_7bit -
    [ch:Integer] [idx:Integer] [g:Graph] auto_14bit -
    [ch:Integer] [idx:Integer] [g:Graph] auto_nonreg -
    [ch:Integer] [idx:Integer] [g:Graph] auto_reg -
    [ch:Integer] [g:Graph] auto_pressure -
    [ch:Integer] [g:Graph] auto_pitch -

Associate a graph with a specific controller that will be controlled automatically to track the graph.  All controllers but `auto_tempo` are specific to a channel and require a `ch` in range 1 to 16 inclusive.  Some controller types have multiple indices available in each channel, given by the `idx` parameters.  If a given controller already has a graph associated with it, the new graph replaces the existing graph.

For the `auto_nonreg` and `auto_reg` controllers, `idx` must be in range zero to 0x3FFF inclusive.  For the `auto_7bit` controller, the `idx` must be in range 0x40 to 0x5F inclusive, or 0x66 to 0x77 inclusive.  For the `auto_14bit` controller, the `idx` must be in range 0x01 to 0x1F inclusive.

## Rendering operations

    [sect:Set] [layer:Set] [art:Set] [v:Articulation] note_art -
    [sect:Set] [layer:Set] [art:Set] [v:Ruler] note_ruler -
    [sect:Set] [layer:Set] [art:Set] [v:Graph] note_graph -
    [sect:Set] [layer:Set] [art:Set] [v:Integer] note_channel -
    [sect:Set] [layer:Set] [art:Set] [v:Integer] note_release -
    [sect:Set] [layer:Set] [art:Set] aftertouch_enable -
    [sect:Set] [layer:Set] [art:Set] aftertouch_disable -

Add classifiers to the note rendering pipelines.  See the documentation on note rendering for the details of how pipelines and classifiers work.  Each of these operations requires three sets, the first selecting the NMF section(s) the classifier applies to, the second selecting the NMF layer(s) within those section the classifier applies to, and the third selecting the articulation(s) within those layers the classifier applies to.

The `note_art` `note_ruler` and `note_graph` operations are used to determine the articulation for measured notes, the ruler for unmeasured grace notes, and the graph that determines note-on velocity and aftertouch if it is enabled.

The `note_channel` determines the MIDI channel notes are assigned to.  It must have a value in range 1 to 16 inclusive.

The `note_release` determines the note-off velocity used for the note, which must be in range 0 to 127 inclusive.  Or, it can have the special value -1, which means that a note-on message with velocity zero should be used.

The `aftertouch_enable` and `aftertouch_disable` either enable or disable aftertouch for the notes the classifier applies to.

The order in which classifiers are declared is significant.
