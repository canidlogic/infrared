# Infrared script specification

Infrared transforms an input file in Noir Music Format (NMF) into an output file in MIDI (.MID) format.  The details of this transformation are controlled by an Infrared script.  This document specifies the format of the Infrared script.

## Data types

Infrared scripts support a few different data types which are described in the subsections below.

Some of the data types are _primitive_ while others are _objects._  Primitive data types are stored directly within the interpreter memory cells and copied around.  Object data types are passed by reference.

Infrared has a simple memory management strategy for objects.  Each allocated object has a lifetime that proceeds all the way to the end of interpretation.  No attempt is made to garbage-collect unreachable objects before then.

### Integer

The _integer_ data type is a signed, 32-bit integer.  It has a range of -2,147,483,647 to 2,147,483,647 (inclusive).  Note that the two's-complement least negative value of -2,147,483,648 is _not_ in this integer range.  This is a primitive data type.

### Text

The _text_ data type is an ASCII string of text.  Only visible, printing ASCII characters and the ASCII space are allowed (range 0x20 to 0x7E inclusive).  The length of a text object must be in range zero to 1023 characters.  Text objects are immutable after definition.

### Blob

The _blob_ data type is a binary string.  It may include any byte values including zero and including byte values in range 0x80 to 0x7F.  The length of a blob must be in range zero to 1,048,576 bytes.  Blob objects are immutable after definition.

### Graph

The _graph_ data type is an object that tracks the change of a parameter value over time.  See the separate documentation about graphs for further information.  Graph objects are immutable after definition.

### Set

The _set_ data type is an object that stores a set of unique integer values that are each zero or greater.  This is used in pipelines for selecting specific note events.  See the separate documentation about note rendering for a specific definition of set objects.  Set objects are mutable.

### Articulation

The _articulation_ data type is an object that determines how the notated duration of measured NMF notes is transformed into the performance duration.  It is specified in the note rendering documentation.  Articulations are immutable after definition.

### Ruler

The _ruler_ data type is an object that determines how unmeasured grace notes are transformed into a performance time offset and a performance duration.  It is spercified in the note rendering documentation.  Rulers are immutable after definition.

### Pointer

The _pointer_ data type represents a logical time position within the performance, which is used to position MIDI events defined within the Infrared script.  The pointer can either have a special "header" value that makes it a _header pointer_ or it can define a specific moment offset in a logical format.

See the separate documentation about the MIDI output system for the definition of the header buffer, the moment buffer, and moment offsets.

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

The _moment part_ is either start-of-moment, middle-of-moment, or end-of-moment.  See the MIDI output system documentation for further information.

A non-header pointer can be translated into a moment offset by the following procedure:

First, look up the quantum offset of the section from the NMF file, with an error if the indicated section number is not defined.

Second, add the quantum offset within the pointer to the result of the previous step.

Third, multiply the result of the previous step by 8 to convert it into a subquantum offset.

Fourth, if the grace pickup is not zero, use the grace pickup index with the ruler object to get the offset of the indicated unmeasured grace note, as explained in the section about rulers.  If the grace pickup is zero, ignore this step.

Fifth, add the tilt to the subquantum offset computed in previous steps.

Sixth, multiply the result of the previous step by 3 and add the moment part to get the moment offset, using 0 for start-of-moment, 1 for middle-of-moment, or 2 for end-of-moment.

The result of these steps might be a negative time offset, which is acceptable within the Infrared sequence.

Pointer types are objects, and furthermore they are mutable.

## Interpreter state

Infrared scripts are run in the context of _interpreter state._  The interpreter state always starts out in the same initial state.  The Infrared script then makes modifications to this initial state.  At the end of the Infrared script, the final interpreter state is then the state that is used to transform the input NMF file into an output MIDI file.

### NMF sections

The input NMF object must be loaded into memory before interpretation, but during script interpretation the only relevant data is the section table.  The section table is needed so that pointer data types can be properly computed as time offsets.

### MIDI output system

The MIDI output system state is described in detail in separate documentation.  For the purpose of Infrared scripts, the header and moment buffers are the relevant pieces of state.  Manual events can be scheduled in the Infrared script in both buffers.  Both buffers start empty.

After the script completes, note events transformed from the input NMF file will be added to the moment buffer automatically using the process described in the separate documentation about note rendering.  Automatic control messages will also be added to the moment buffer using the process described in the separate documentation about the control system.

### Note rendering system

The note rendering system is described in detail in separate documentation.  For the purpose of Infrared scripts, the pipelines are the relevant pieces of state.  The Infrared script may add classifiers to each of the pipelines, which start out empty with no classifiers.  These classifiers then affect how NMF note events are transformed into MIDI note events.

### Control system

The control system is described in detail in separate documentation.

Manual events are directly inserted into the header or moment buffer, which is part of the MIDI output system.

The control system also has a tempo graph and controller graphs.  The tempo graph is a reference to a graph object that controls the tempo throughout the performance.  The controller graphs can associate graph objects with specific controllers.  For both the tempo and controller graphs, Infrared will automatically generate all the necessary controller messages to track changes in values, as described in the separate documentation.
