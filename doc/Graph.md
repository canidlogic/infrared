# Infrared graph objects

Infrared graphs are used to describe the changes of parameter values over time.  Parameter values are integers that are zero or greater.

A graph is defined by a sequence of one or more _nodes._  Each node has a unique integer subquantum offset.  (See the document on timing systems for further information about subquanta.)  Each node is one of three types:

1. Constant
2. Pulse
3. Ramp

The _constant node_ uses the same constant integer value for all time points starting at the node subquantum offset and proceeding to one subquantum before the next node's subquantum offset.  If the constant node is the last node, all time points starting at the node subquantum offset and proceeding to positive infinity have the constant value.  If the constant node is the first node, all time points prior to the node subquantum offset also have the constant value.

The _pulse node_ is similar to the constant node, except at the node subquantum offset, there is a transition to a pulse value and then to the node's constant value all within a single time point.  This is intended for controllers such as sustain pedals, where the pedal should be cleared and immediately reactivated.  In certain contexts where pulses are not supported, such as note velocities and tempo settings, pulse nodes are treated as constant nodes and their pulse is ignored.

The _ramp node_ is a smooth transition between two different integer values A and B.  The ramp node has the value A at the node's subquantum offset.  The ramp node interpolates values such that the value B would come at the subquantum offset immediately following the last subquantum in the ramp.  The last node in a graph may not be a ramp node, because the ramp node requires a defined duration.

Ramp interpolation is controlled by a _step value._  The step value is a duration in subquanta that must be greater than zero.  The ramp value updates at the node's time offset and also at any time offset within the ramp that is divisible by the step value.

Each ramp has a flag indicating whether it is a linear ramp or a logarithmic ramp.  Linear ramps just use linear interpolation.  Logarithmic ramps use a logarithmic scale, defined as follows:

    rl(t) = e^(ln(A + 1) + (t * (ln(B + 1) - ln(A + 1)))) - 1
    where:
      e^ is exponential function
      ln is natural logarithm (base e)
      A is starting ramp value
      B is ending ramp value
      t in range [0.0, 1.0)

The _t_ value in this equation has t=0.0 as the subquantum offset of the node and t=1.0 as the subquantum offset of the next node.

## Data structure

Graphs are stored in a compact memory format.  They consist of an index and a table.  The index contains records that have a subquantum offset of a node and another integer that packs both a table index and a type field of four possible values.  The following types are selected by the type field:

1. Constant node
2. Pulse node
3. Linear ramp node
4. Logarithmic ramp node

The table contains the integer parameters for each node.  Constant nodes have a single parameter, which is the constant value.  Pulse nodes have two parameters:  the constant value and the pulse value.  Ramp nodes have three parameters:  the starting value, the ending value, and the step size.

## Graph construction

Graph objects are constructed in Infrared scripts using an accumulator register.  When a new graph object begins, it is held in the accumulator register with no nodes defined.  There are then operations to create the individual nodes, which must be defined in chronological order.  Non-header pointer data types are used to specify the node offsets.  Finally, the graph object is finished by taking the completed object from the accumulator and pushing it onto the stack.

## Constant graph cache

Graphs representing constant values are cached so that a single constant-value graph can be shared for each constant value.  A special constant graph operator is provided to Infrared scripts to enable constant graph sharing.  The operator simply takes the desired constant value and then returns the shared graph for that value.

The constant graph cache is implemented with a growable array where the array index is the desired constant value and each value is either a pointer to a graph object for that value or NULL if the graph has not yet been allocated.  There is a maximum constant value to prevent the array from being too large.  If a constant value is requested beyond the maximum, a new graph will be silently created instead of using the cache.

## Queries

Graph objects support two different kinds of queries.

The first query, a _time query,_ takes a subquantum offset as input.  It then returns the integer value of the graph at the requested time offset.

The second query, a _range query,_ takes as input a time range denoted by a minimum subquantum offset and a maximum subquantum offset, where the maximum must be greater than or equal to the minimum.  The range query then looks for any time points within the specified time range where the value of the graph changes.  If there are no changes of value within the range, the range query returns an empty result.  Otherwise, the range query returns the time offset and new value of the earliest value change within the range.

## Tracking algorithm

Given a graph and a time range, it is often necessary to derive a sequence of value changes within that time range according to the graph.

For controller values, tempo changes, and other such global parameters, the usual strategy is to use a time query to get the initial value at the start of the performance and then use the tracking algorithm to get any additional messages required to change the value throughout the performance.  The tracking algorithm in this case starts with a range that begins one subquanta after the start of the performance and ends on the last subquantum of the performance.

For polyphonic aftertouch, a time query is used to get the velocity of the note-on event.  Then, the tracking algorithm is used with the initial range starting at the subquantum immediately following the note-on event and ending at the subquantum immediately preceding the note-off event.

The tracking algorithm begins with an initial time range and continues until this initial time range is empty.  A range query is used to get the earliest change in value within this time range.  If there are no change of values, then the tracking algorithm is done.  Otherwise, use the earliest change of value to add an appropriate value change message and then update the time range so that the new lower boundary is one subquantum beyond the change of value that was just processed.  Then, loop back with this new time range.
