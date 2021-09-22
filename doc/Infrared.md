# Infrared program documentation

- &#x200b;1. [Introduction](#mds1)
- &#x200b;2. [Syntax](#mds2)
- &#x200b;3. [Retro template file](#mds3)
  - &#x200b;3.1 [Template command lines](#mds3p1)
  - &#x200b;3.2 [Stream commands](#mds3p2)
  - &#x200b;3.3 [Cue mode commands](#mds3p3)
  - &#x200b;3.4 [Interpretation of cues](#mds3p4)
  - &#x200b;3.5 [Rate and frame command](#mds3p5)
- &#x200b;4. [Compilation](#mds4)

## <span id="mds1">1. Introduction</span>

The Infrared program is a bridge between the [Noir Music Format](https://www.purl.org/canidtech/r/libnmf) (NMF) and the [Retro](https://www.purl.org/canidtech/r/retro) synthesizer.

The input to Infrared is the following three items:

1. Retro template script
2. NMF file
3. Note rendering Lua script

As output, Infrared generates a Retro synthesis script that can be given directly to the Retro synthesizer and rendered into a WAV audio file.

## <span id="mds2">2. Syntax</span>

The syntax of the Infrared program is as follows:

    infrared (-crlf) [template] [nmf] [note_script]

If the optional `-crlf` option is present, then Infrared will use CR+LF line breaks in the generated output.  Otherwise, Infrared will use LF line breaks.

`[template]` is the path to a Retro template file.  The format of this template file is described in &sect;3 [Retro template file](#mds3).

`[nmf]` is the path to an NMF file that defines the notes and cues in the composition.  Infrared only accepts NMF files that have the following restriction:

- Quantum basis must be one of the following:
  - 44,100 quanta per second, OR
  - 48,000 quanta per second

See [libnmf](https://www.purl.org/canidtech/r/libnmf) for more about the NMF format, and see [Halogen](https://www.purl.org/canidtech/r/halogen) for how to transform NMF files with 96 quanta per quarter note into one of the fixed quantum bases above.

`[note_script]` is the path to a [Lua](https://www.lua.org/) script that contains a note rendering routine.  This specifies how a sequence of NMF notes is transformed into a sequence of Retro note events.  See `NoteRendering.md` for the specifics of how this Lua script works.

## <span id="mds3">3. Retro template file</span>

The _Retro template file_ is a text file template that tells Infrared how to generate the desired Retro synthesis script on output.  The Retro template file is basically a normal Retro synthesis script with a few template extensions added.  The template extensions allow Infrared to automatically insert data into the output Retro synthesis script based on the contents of the NMF file that Infrared was given.

See [Retro](https://www.purl.org/canidtech/r/retro) for more information about how a Retro synthesis script is constructed.  This section describes the template extensions that are specific to Infrared.

Note that Infrared does not actually parse the Retro commands within the Retro template file.  Infrared only parses the template extensions defined in this section and uses them to generate the Retro synthesis script output.  The Retro commands will not actually be parsed until the generated Retro synthesis script is sent to the Retro synthesizer.

Infrared will pass through to output but otherwise ignore a UTF-8 Byte Order Mark (BOM) if it is present at the start of the template file.  Infrared only understands the US-ASCII character set, but it ignores and passes through any characters outside of US-ASCII.  You may safely pass UTF-8 through Infrared both with or without a Byte Order Mark, though Infrared does not understand UTF-8.

Line breaks in the Retro template file may either be Line Feed (LF), Carriage Return followed by Line Feed (CR+LF), or any combination of those two styles.  Carriage Return (CR) characters may only occur immediately before an LF character or an error occurs.  Both line break styles will be converted to LF line breaks in the output, unless the `-crlf` option was provided to Infrared, in which case all line breaks will be CR+LF in output.

### <span id="mds3p1">3.1 Template command lines</span>

Any line in the Retro template file that begins with a grave accent is a _template command line_.  No whitespace is allowed before the grave accent; if there is whitespace before the initial grave accent, the line is __not__ treated as a template command line.

In case an output line in the Retro synthesis script actually needs to begin with a grave accent, there is a special template command escape.  If a template command line begins with a sequence of two or more grave accents, then the template command is interpreted as a command to print the line to output with one less grave accent in the opening sequence of grave accents.  This allows existing Retro synthesis scripts to be escaped by simply prefixing a grave accent to any line that begins with a grave accent, thereby escaping such lines.

If a template command line is empty except for the opening grave accent, or a template command line begins with a grave accent followed by either a space or a horizontal tab, then the template command line is a template comment.  Template comments are ignored by the template processor and are not included in output.

The following sections describe the template command lines that are available, besides the escape and comment lines described in this section.

### <span id="mds3p2">3.2 Stream commands</span>

A template command line that begins with a grave accent followed immediately by an uppercase `S` is a _stream command_.  After the uppercase `S`, nothing further may be present on this template command line except for spaces and horizontal tabs.

At most one stream command may be present in a Retro template file.  It is an error if there is more than one stream command.  However, it is acceptable for a Retro template file to have no stream commands.

When a stream command occurs, Infrared will write a sequence of zero or more Retro note events, each note event on a separate line of text.  These Retro note events are determined by taking all the non-cue notes in the input NMF file, running them through the provided note rendering Lua script, and streaming any resulting Retro note events into output at the location determined by the stream command.

Infrared follows the format of the `n` Retro operation for defining the note events.  See the documentation of [Retro](https://www.purl.org/canidtech/r/retro) for more about the format of Retro note events.

If no stream command is present in the Retro template file, then Infrared will still parse the NMF file and use the Lua script, but the generated Retro note events will never be included in output.

### <span id="mds3p3">3.3 Cue mode commands</span>

Infrared supports _cue mode_ to allow the sample offsets of cues from the NMF file to be inserted into output.  At the start of a Retro template file, cue mode is always turned off.

To turn cue mode on, include a template command line that begins with a grave accent followed immediately by an uppercase `C`.  To turn cue mode off, include a template command line that begins with a grave accent followed immediately by a lowercase `c`.  Neither command line allows anything after the letter except for spaces and horizontal tabs.

Turning on cue mode when cue mode is already on has no effect.  Turning off cue mode when cue mode is already off has no effect.  Cue mode may be either on or off at the end of the Retro template file.

When cue mode is on, text lines in the Retro template file that are not template command lines will be processed with _cue escaping_.  Cue escaping, however, __never__ applies to template command lines for escaped lines that begin with a grave accent (see &sect;3.1 [Template command lines](#mds3p1)).

Cue escaping looks for grave accent characters within the lines that it applies to.  If there are multiple grave accent characters, they are processed from left to right.  Grave accent characters that are followed immediately by another grave accent character are paired with the following grave accent character and are interpreted as an escape for a literal grave accent character in the output.

Grave accent characters that are not part of a paired literal grave accent escape are interpreted as the start of a cue.  The cue begins with the opening grave accent character and proceeds up to and including the next semicolon `;` character.  If the end of the line or another grave accent occurs before the next semicolon character, or there is no next semicolon character on the line, there is a template parsing error.

Cues are replaced with a sequence of one or more ASCII decimal digits that indicate the sample offset at which the cue occurs.  The determination of the sample offset is described in the next section.

### <span id="mds3p4">3.4 Interpretation of cues</span>

Cues always begin with a grave accent that is not immediately followed by another grave accent in template lines that are not template commands when cue mode is on.  Cues always end with a semicolon character `;`.  See &sect;3.3 [Cue mode commands](#mds3p3) for further information.

A _full cue_ has the following format:

1. Grave accent
2. Section number
3. Period character `.`
4. Cue number
5. Semicolon `;`

The section number is an unsigned decimal integer that specifies a section number within the NMF file.  The cue number is an unsigned decimal integer that specifies a cue number within that section of the NMF file.

The full cue must refer to a cue event that is defined exactly once in the NMF file.  The cue event must be a note with the following format:

1. Time offset is the time of the cue.
2. Duration is zero.
3. Pitch is zero.
4. Section index is the section the cue is in
5. Articulation and layer are the cue number (see below)

Duration __and__ pitch must both be zero for the note to be recognized as a cue.  If the duration is zero but the pitch is non-zero, the NMF note is __not__ recognized as a cue and is ignored.

The cue number is split across the articulation and layer fields.  The layer field is interpreted as an unsigned 16-bit integer value.  The articulation field is treated as an unsigned 6-bit integer value, except the values 62 and 63 are not allowed.  The cue number is the value that results when the layer field is interpreted as the 16 least significant bits and the articulation field is treated as the 6 most significant bits of a 22-bit integer value.  Bearing in mind the restriction that the articulation may not have values 62 or 63, this scheme allows for up to 4,063,232 distinct cues within each section.

Infrared will go through all the notes in the NMF file and build an index of all cues and what sample offsets they occur at.  It is an error if a specific cue number within a specific section occurs more than once.  It is also an error if a full cue event references a cue that is not present in the NMF file.

A full cue is replaced in output with a decimal integer that indicates the sample offset of the cue.  This allows cues within the Retro template file to be used to refer to specific locations defined within the NMF composition.

There is also a _partial cue_ which has the following format:

1. Grave accent
2. Cue number
3. Semicolon `;`

Partial cues may only occur after at least one full cue has occurred somewhere before them.  The section number of the partial cue is automatically taken to be the section number that was in the last full cue.  The memory of the last cue is not affected by turning cue mode on and off.

### <span id="mds3p5">3.5 Rate and frame command</span>

Infrared provides a template command that handles both the `%rate` and `%frame` commands in the heaer of a Retro synthesis script.  Using this template command allows for the Retro template file to be independent of any particular sample rate.  The sample rate from the NMF file will be used for the `%rate` command and Infrared will convert frame durations from quantities given in milliseconds in the template to sample counts in the generated synthesis script.

To use this feature, include a template command line that begins with grave accent followed by an uppercase `R`.  This template command can be included at most once in a template or an error occurs.  The template command will be replaced in output with an appropriate `%rate` command followed by an appropriate `%frame` command.

The syntax of the rate and frame template command is as follows:

1. Grave accent
2. Uppercase `R`
3. Zero or more spaces and tabs
4. One or more decimal digits
5. Zero or more spaces and tabs
6. Comma `,`
7. Zero or more spaces and tabs
8. One or more decimal digits
9. Zero or more spaces and tabs

The first sequence of decimal digits decodes to an unsigned integer quantity that gives the number of milliseconds of silence written before the synthesized sound begins.  The second sequence of decimal digits decodes to an unsigned integer quantity that gives the number of milliseconds of silence written after the synthesized sound ends.  Either or both quantities may be zero.

## <span id="mds4">4. Compilation</span>

Infrared has two dependencies:

1. `libnmf` for reading NMF files
2. `liblua` for interpreting Lua scripts

`libnmf` is available [here](https://www.purl.org/canidtech/r/libnmf).

`liblua` is the embedded Lua interpreter library.  Infrared is written against Lua version 5.4.  See the [Lua website](https://www.lua.org/) for information about how to download and compile the Lua library.

You may also need to link in the math library with `-lm` depending on which platform you are on.

All you have to do is compile Infrared with the two dependencies.  The following is an example build line for `gcc` (all on one line):

    gcc -O2 -o infrared
      -I/path/to/libnmf/include
      -I/path/to/liblua/include
      -L/path/to/libnmf/lib
      -L/path/to/liblua/lib
      infrared.c
      -lnmf -llua -lm

You will need to adjust the `-I` line(s) to the directory or directories that contain the `libnmf` and `liblua` header files.  You will need to adjust the `-L` line(s) to the directory or directories that contain the `libnmf` and `liblua` static libraries.  You may need to adjust the name of `-llua` if the Lua library file is not named `liblua.a`
