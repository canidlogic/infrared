# Infrared note rendering

This document describes the way that Infrared renders notes from Noir into Retro events.

Since the specific way in which Noir notes map to Retro note events is specific to a particular composition, Infrared does not specify a fixed transformation.  Instead, Infrared takes a Lua script as input and uses the Lua script to render notes.  This allows specific compositions to control exactly how Noir notes are rendered into Retro note events by providing an appropriate Lua script.

## Note rendering function

This note rendering script must declare a function `note` that takes the following parameters:

1. Sampling rate in Hz
2. Sample offset at which note starts
3. Duration of note in samples
4. Pitch of the note (see below)
5. NMF articulation
6. NMF section that note belongs to
7. NMF layer within the section

Parameter (7) refers to the layer within the NMF file, __not__ the layer within the output Retro script.  Layers mean different things in NMF and Retro.

The sample offset is always zero or greater, and the duration is always greater than zero.  NMF cue events with a duration zero are never passed through to the note renderer, and Infrared does not allow grace notes with negative durations, either.

The pitch of the note refers to the number of equal-tempered semitones away from middle C, with positive values being higher in pitch and negative values being lower in pitch.  The range is that of an 88-key piano keyboard, which results in a valid range of [-39, 48].

The NMF articulation is always in range [0, 61].  The NMF section is always in range [0, 65534].  The NMF layer is always in range [1, 65536].  Again, the layer refers to the NMF layer, not the Retro layer!

This note rendering function is called for every non-cue note that is encountered in the NMF file Infrared is rendering.  Infrared only allows NMF files with fixed quantum bases of 44100 Hz or 48000 Hz.  The sampling rate parameter (1) will always have the same value across all calls to the `note` function.  However, there are no ordering guarantees for the order in which NMF notes are passed through to the note rendering script.  In particular, notes are __not__ guaranteed to be in any chronological order.

## Event output function

Infrared defines a Lua function named `retro_event` that can be used to output Retro note events.  Events can be output in any order.  This function `retro_event` takes the following parameters:

1. Sample offset at which note starts
2. Duration of note in samples
3. Pitch of the note
4. Retro instrument number
5. Retro layer

The first three parameters have the same interpretation as for the `note` function described in the previous section.  The fourth and fifth parameters have minimum values of one and should refer to instruments and layers that will be defined in the Retro synthesis script.

There does not need to be a one-to-one correspondence between NMF notes and generated Retro note events.  If the `note` function never calls `retro_event` for a particular invocation, then the NMF event will be filtered out.  If the `note` function calls `retro_event` once for a particular invocation, it will transform an NMF note into a Retro note event.  The `note` function can even call `retro_event` multiple times in a single invocation to generate multiple Retro note events for a single NMF note event.

## Example note rendering script

Here is a simple Lua note rendering script that just passes the `t` `dur` and `pitch` parameters through from NMF to Retro, and sets all the Retro instruments and layers to one:

    function note(rate, t, dur, pitch, art, sect, layer)
      retro_event(t, dur, pitch, 1, 1)
    end

You can test how note rendering scripts work with the `irtest` utility program.  See that program source code for further information.
