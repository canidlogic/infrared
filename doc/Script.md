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

The _set_ data type is an object that stores a set of unique integer values that are each zero or greater.  This is used in pipelines for selecting specific note events.  See the separate documentation about note rendering for a specific definition of set objects.  Set objects are immutable after definition.

### Articulation

The _articulation_ data type is an object that determines how the notated duration of measured NMF notes is transformed into the performance duration.  It is specified in the note rendering documentation.  Articulations are immutable after definition.

### Ruler

The _ruler_ data type is an object that determines how unmeasured grace notes are transformed into a performance time offset and a performance duration.  It is specified in the note rendering documentation.  Rulers are immutable after definition.

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

The interpreter state is divided into the following specific groups:

1. Core state
2. Pointer state
3. Ruler stack
4. Diagnostic state
5. Rendering state

The _core state_ are those parts of interpreter state that are required for the Shastina interpreter to work.

The _pointer state_ allows pointer objects to be computed against the input NMF file.

The _ruler stack_ is used to track which ruler is assigned to grace note pointers.

The _diagnostic state_ allows the Infrared script to print diagnostic messages.

The _rendering state_ allows the Infrared script to control the details of how the input NMF file will be transformed into the output MIDI file.

The following subsections describe these state groups in further detail.

### Core state

The core interpreter state consists of the interpreter stack and the memory bank storing variables and constants.

Both the stack and memory bank store data values in a _variant_ type that can hold any of the data types specified earlier.  The stack supports the grouping system specified by Shastina.  At the end of script interpretation, the interpreter stack must be empty.

The core state is fundamental during script interpretation, but following completion of interpretation, the core state is no longer relevant.

### Pointer state

The pointer state is loaded with the parsed NMF data at the start of interpretation.  This is necessary for the NMF section table, which is required for pointer objects to be computed into moment offsets during script interpretation.

Pointer state is read-only in that the parsed NMF data can not be changed during script interpretation.  Following the completion of script interpretation, the pointer state is no longer relevant since all relevant pointers will have been converted to moment offsets or header pointers by that point.

### Ruler stack

The ruler stack is a stack of ruler objects that is used during interpretation.  It starts out empty.  It does not need to be empty at the end of interpretation (unlike the main interpreter stack).

The _current ruler_ is determined by the ruler stack in the following way.  If the ruler stack is not empty, then the current ruler is whichever ruler is on top of the ruler stack.  If the ruler stack is empty, then the current ruler is a default ruler that has a slot of 48 subquanta and a gap of zero.

Infrared operations are provided to push and pop rulers from the ruler stack.  The current ruler is used for numeric literals suffixed with `g` to determine the ruler applied to grace note offsets.

### Diagnostic state

The diagnostic state provides two output functions.  The first prints the value of any data type to standard error.  Printing a string will simply print whatever text is stored within the string object.  The second output function adds a line break to standard error.

The diagnostic state also includes a newline flag.  The newline flag starts out set.  When a data object is printed to standard error, the newline flag is checked.  If the newline flag is set, then a header will be printed at the start of the line identifying the name of the executable module and the line number in the script.  This ensures that all non-blank lines printed to standard error will have a header indicating where the message is from.

The newline flag is always cleared after printing a data value, and always set after printing a line break.  If at the end of script interpretation the newline flag is clear, a line break will automatically be printed to standard error to close the last diagnostic line.

### Rendering state

The rendering state receives instructions from the Infrared script that determine how the NMF will be rendered to MIDI.

The internal API that scripts use for setting the rendering state is contained in the `control.h` and `render.h` headers.  However, the `render_nmf()` and `control_track()` functions are not directly accessible from Infrared scripts.

This rendering state allows classifiers to be registered that control how NMF notes are translated into Infrared notes.  It also allows certain events to be manually inserted.  Finally, it allows graphs to be associated with specific controllers, which will then be automatically controlled by Infrared so that they track the graphs.

## Header line

The first Shastina entities in an Infrared script must be the start of a metacommand, the metacommand token `infrared` (case sensitive), and the end of a metacommand.  In other words, the header looks like this:

    %infrared;

If there are other entities in this header metacommand after the initial `infrared` token, the Infrared interpreter assumes the script is for an unsupported future version of Infrared and stops on an error.

## Entity interpretation

The following subsections describe how Infrared interprets the different kinds of Shastina entities that it supports.

### Numeric entities

Shastina numeric entities function differently in Infrared scripts depending on whether they have a single-letter suffix.  All numeric entities begin with an optional sign character, followed by a sequence of one or more decimal digits, and then an optional single-letter suffix, which is case-sensitive.  The range of signed decimal values always matches the range of an Infrared integer described earlier.

If there is no single-letter suffix, then the numeric entity has the effect of pushing an integer value on top of the interpreter stack.

If there is a single-letter suffix, then there must be a pointer object on top of the stack.  The entity literal will modify the location of that pointer object and leave it on top of the stack.  No integer is pushed on top of the stack in this case.

The following single-letter suffixes are supported:

- `s` : Jump to a section
- `q` : Seek to a specific quantum offset
- `r` : Seek relative to current quantum offset
- `g` : Set a grace note offset
- `t` : Tilt by a subquantum offset
- `m` : Set the moment part

The `s` suffix is the only kind that can be applied to a header pointer.  All other suffixes require the pointer to be a non-header pointer.

Using the `s` suffix will jump to the start of the indicated NMF section and reset all other pointer fields except for the moment part.  If the `s` suffix is used on a header pointer, it will be converted into a non-header pointer and its moment part will be initialized to middle-of-moment.

The `q` and `r` suffixes change the quantum offset within the selected NMF section.  `q` sets an absolute offset, while `r` changes the current offset by a signed, relative amount.  Both suffixes clear the grace note offset and tilt to zero.

The `g` suffix sets a specific grace note offset.  If it is zero, then the pointer is on the beat and not a grace note.  Otherwise, it must be less than zero.  -1 selects the grace note immediately before the beat, -2 selects the grace note immediately before that, and so forth.  The ruler used for computing grace note offsets is determined by the current ruler of the ruler stack at the time that the `g` suffix is invoked.  The `g` suffix clears the tilt to zero.

The `t` suffix sets a subquantum offset that will be applied after everything else is computed.

Finally, the `m` suffix determines the moment part of the pointer.  It must be either `-1m` for start of moment, `0m` for middle of moment, or `1m` for end of moment.

Operations are also provided that perform the same pointer arithmetic as these suffixes.  This is useful when computations need to be performed on the integers.  See the separate operations specification for further information.

### String entities

Shastina string entities cause either a text object or a blob object to be constructed and pushed on top of the interpreter stack.

If the string entity uses double quotes, then an Infrared text object will be constructed and pushed on top of the stack.  Only visible, printing ASCII characters and the space are allowed.

The backslash character is used for escape sequences within text literals.  Backslash followed by another backslash is the escape for a literal backslash.  Backslash followed by double quote is the escape for a literal double quote.  No other escape sequences are supported.

If the string entity uses curly braces, then an Infrared blob object will be constructed and pushed on top of the stack.  The binary data is encoded in base-16 using either lowercase or uppercase (or both) letters.  Pairs of base-16 digits forming individual bytes must not have any separation.  However, whitespace separation including space, tab, and line breaks are optional and allowed everywhere except within a digit pair.

Neither string entity type allows for prefixes to the opening quote or curly brace.

### Memory entities

Infrared supports the Shastina entities that allow variables and constants to be declared, and also the entities for getting the value of variables and constants, and setting the value of variables.

Variable and constant names must be sequences of one up to 31 ASCII alphanumerics and underscores, where the first character must be a letter.  Names are case sensitive, and both variables and constants share the same name space.  Variables and constants must be declared before they can be used.  The declaration pops a value off the top of the interpreter stack to use as the initial value.  Any of the Infrared data types may be stored in variables and constants.

Note that pointer objects are mutable, so if a pointer is stored in a constant and then modified separately, the pointer value in the constant changes also.  To prevent this, a new pointer should be made and copy the original pointer value, so that changes will not cascade to the memory value.

The Shastina "get" entity can be used to push a copy of any variable or constant on top of the interpreter stack.  The Shastina "assign" entity can be used to pop a new value from the top of the interpreter stack and overwrite the current value assigned to a variable.  The "assign" entity is not allowed to be used with constants.

### Grouping entities

Infrared supports the Shastina grouping entities.  Beginning a group hides everything currently on the interpreter stack.  Ending a group requires there to be exactly one non-hidden element on the interpreter stack.  The hidden elements are then restored.  Groups may be nested.

Infrared also supports Shastina array entities.  Shastina creates implicit groups for each array element, except in the special case of an empty array.  At the end, Infrared pushes the count of array elements onto the interpreter stack as an integer value.

### Operation entities

Shastina operation entities cause the Infrared interpreter to perform various operations.  These operations can pop elements off the interpreter stack for input, push elements onto the interpreter stack for output, and make modifications to the ruler stack, diagnostic state, and rendering state.

Infrared uses an internal registry module that stores a mapping of case-sensitive operation names to functions that perform the operation.  Operations are then packaged into operation modules.  The operation modules each have a registration function that registers each of their operations with the internal registry.  In the main Infrared module, each of the registration functions of the various operation modules are invoked at the beginning of execution to register all the operations.

The operation modules are documented separately.
