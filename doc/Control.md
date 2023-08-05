# Infrared control system

There are two basic sources of messages that are passed into the MIDI output module.  The first is note rendering, which transforms NMF note events into MIDI events.  (See separate documentation for details.)  The other source for messages is the _control system,_ which is described in this document.

The control system consists of manual events and automatic events.

## Manual events

_Manual events_ are events that are directly defined by the Infrared script and passed directly to the MIDI output module.  The following kinds of events are handled as manual events:

1. Null events
2. Text events
3. Time signature events
4. Key signature events
5. Custom meta events
6. System exclusive events
7. Instrument events
8. Modal events

Each of these event types are defined in the MIDI output module documentation, except for instrument events and modal events, which are defined in the following subsections.

### Instrument events

Instrument events are used to set the instrument patch used for performing on a specific channel.  Instrument events always include the program number of the desired instrument, which is in range 1 to 128 inclusive.  Instrument events may optionally include a bank number, which selects a program number from a specific bank of instruments.

Instrument events that have only a program number are implemented as a Program Change message.

Instrument events with a bank number are implemented as two Control Change messages followed by a Program Change message.  The two Control Change messages set the most significant and least significant bytes of the bank (in that order), while the Program Change messages sets the program number within that bank.

### Modal events

Modal events are actually Control Change channel events, but they use the reserved controllers 120 to 127, which are for setting certain special modes instead of control values.  The supported modal events are:

1. All Sound Off
2. Reset All Controllers
3. Local Control On
4. Local Control Off
5. All Notes Off
6. Omni Mode Off
7. Omni Mode On
8. Mono Mode On
9. Poly Mode On

The Mono Mode On modal event takes a parameter counting the number of channels to use, or the special parameter value of zero.  None of the other modal events have parameters.

The meaning of these modal events are defined in the MIDI specification.

## Automatic events

All other control events are _automatic events,_ which means they are not directly defined by the Infrared script.  Instead, the Infrared script defines a graph object and attaches the graph object to a specific controller.  The tracking algorithm will then be used to derive the necessary controller messages so that the control value will be automatically tracked throughout the performance.

See the separate graph documentation for further information about graphs and the tracking algorithm.

### Tempo graph

The _tempo graph_ is used for controlling the tempo.

The value of the tempo graph is the number of microseconds in a quarter note.  The tempo will always be set at the beginning of the performance with a time query, and the tracking algorithm described in the graph documentation will be used to determine where to insert Set Tempo events.

### Controller graphs

The _controller graphs_ are used for various controllers, excluding the channel mode messages and bank select, which are handled with manual events as described earlier.

There are a few different controller types defined by the MIDI standard:

1. 7-bit controllers
2. 14-bit controllers
3. Non-registered controllers
4. Registered controllers
5. Channel pressure controllers
6. Pitch bend controllers

The _7-bit controllers_ are set with a single Control Change message.

The _14-bit controllers_ are set with two Control Change messages, one with the most significant bits and the other with the least significant bits.

The _non-registered controllers_ are set with four Control Change messages.  The first two select a non-registered controller index and the second two set a 14-bit value.

The _registered controllers_ are set with four Control Change messages.  The first two select a registered controller index and the second two set a 14-bit value.

The _channel pressure controllers_ use Channel Pressure (Aftertouch) messages.  These are not the same as Polyphonic Pressure (Aftertouch) messages.

The _pitch bend controllers_ use Pitch Bend messages.

Each channel has its own version of these controllers, so the channel must be indicated when selecting the controller.

Graphs can be associated with any of these controller types supported by the MIDI standard.  If a controller for a specific channel has a graph, then it will be set at the start of the performance using a time query to the graph object, and then changes in value will be determined by the tracking algorithm described in the graph documentation.
