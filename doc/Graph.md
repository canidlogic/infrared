# Infrared graph objects

Infrared graphs are used to describe the changes of parameter values over time.  Parameter values are integers that are zero or greater.

A graph is defined by a sequence of one or more _nodes._  Each node associates a unique integer moment offset with the value of the graph at that node.  (See the document on the MIDI output system for a definition of moment offsets.)  Each graph must have at least one node, and when nodes are sorted in chronological order, two adjacent nodes may not map to the same value.

The value of a graph at a moment offset _t_ is equal to the value of the node that has the greatest moment offset that is less than or equal to the _t_ time point.  If there is no node that has a moment offset less than or equal to _t_ then the value of the graph at that time is equal to the value of the first node in the graph.

Since graphs use moment offsets rather than subquantum offsets, it is possible for a graph to have different values at different parts of the same moment.  This is useful for pulse effects.  For example, to clear and set the damper pedal in the same subquantum offset, there can be a graph node clearing the pedal at the start-of-moment position and a graph node setting the pedal at the middle-of-moment position.

## Graph building

Graph objects are built using an accumulator register that allows them to be specified piece by piece.

Graph building proceeds in chronological order.  _Regions_ are appended, and each region has a moment offset.  Some regions require a duration, so regions are buffered and only processed when the next region is declared or when the graph is completed.

Processing a region results in a sequence of records in chronological order mapping moment offsets to graph values at those time points.  These records are then used to append nodes to the graph, but graph nodes are only generated when the value of the graph actually changes.

The following subsections describe the different kinds of regions and how they are translated into graph values.

### Constant regions

Constant regions have the same graph value from their starting moment offset up to but excluding the start of the next region.  If a constant region is the last region in the graph, the constant value continues all the way to infinity.

### Ramp regions

Ramp regions transition between two different integer values A and B.  The ramp regions has the value A at its moment offset.  The ramp region then interpolates values such that the value B would come at the moment offset of the next region.  It is an error for a ramp region to be the last region in a graph, because the ramp requires a following region to determine its duration.

Ramp interpolation is controlled by a _step distance._  The step distance is a duration in subquanta that must be greater than zero.  The graph value changes at the region's starting subquantum offset and also at any subquantum offset that is divisible by the step distance and less than the next region's subquantum offset.  At each of these interpolated positions, the same moment part is used to determine the moment offset as was used in the moment offset at the start of the ramp region.

Each ramp has a flag indicating whether it is a linear ramp or a logarithmic ramp.  Linear ramps just use linear interpolation.  Logarithmic ramps use a logarithmic interpolation, defined as follows:

    rl(t) = e^(ln(A + 1) + (t * (ln(B + 1) - ln(A + 1)))) - 1
    where:
      e^ is exponential function
      ln is natural logarithm (base e)
      A is starting ramp value
      B is ending ramp value
      t in range [0.0, 1.0)

The _t_ value in this equation has t=0.0 as the subquantum offset of the ramp region and t=1.0 as the subquantum offset of the next region.

In short, ramp pieces have the following parameters:

- Starting value "A"
- Target value "B"
- Step distance
- Linear or logarithmic interpolation

### Derived regions

Derived regions copy values from another graph, optionally applying a time offset and optionally transforming each graph value in the derived region.

In addition to specifying the moment offset of the derived region in the graph under construction, derived regions must also specify an existing graph and a moment offset within that graph to begin transferring graph data.  The derived region proceeds until the next region in the graph under construction, or to infinity if it is the last region in the graph.

The tracking algorithm (described later) will be used to determine the nodes that need to be appended to the new graph in order to copy the graph values from the existing graph.

Before the copied graph values are appended to the derived region, they are transformed by the following formula:

    v' = min(max(floor((N * v) / D) + C, A), B) if B >=  0
    v' =     max(floor((N * v) / D) + C, A)     if B == -1

N and D are the numerator and denominator of a scaling value that is multiplied to each input value.  N must be an integer zero or greater and D must be an integer greater than zero.  The result of this is floored to an integer value and then the constant value C is added to this result.  C is an integer with any value, including negative.  Finally, the result is clamped to be inside the range A to B inclusive, where A and B are integers, A greater than or equal to zero and B greater than or equal to A.  B may also have a special value -1 meaning that no upper limit is applied.

In order to copy values from the given graph without any transformation, use N and D values of one, A and C values of zero, and a B value of -1.

## Constant graph cache

A graph that has a single node is a _constant graph_ because it has the same graph value at all time points, regardless of the node's time point.

Constant graphs are useful for controlling parameters that have a single value that does not change over time.  However, creating a new graph object for each constant parameter value would have significant overhead.

To solve this problem, a _constant graph cache_ is transparently used during graph construction.  After a graph is completed in the accumulator, a check is made whether it is a constant graph.  If it is, then another check is made whether a constant graph of that value is already available in the constant graph cache.  If such a graph is already cached, the cached graph object instance is returned instead of a new object instance.  If no such graph is already cached, a new constant graph object is constructed and returned, and also added to the cache.

## Queries

Graph objects support _queries,_ which determine the value of a graph at a given moment offset.

To answer a query at moment offset _t,_ let S be the set of all graph nodes where the graph node's moment offset is less than or equal to _t._  If S is an empty set, then the answer to the query is the value of the graph node with the least time offset.  If S is not an empty set, then the answer to the query is the value of the graph node within S that has the greatest time offset.

## Tracking algorithm

Graph objects also support the _tracking algorithm,_ which reports all changes of graph value over a given time range to a callback function.

The tracking algorithm requires a graph object, a starting moment offset, and a callback function as parameters.  The tracking algorithm optionally takes an ending moment offset and optionally takes a starting graph value.  The callback function receives as parameters a moment offset and the value the graph changes to at that moment offset.  Callbacks are always performed in chronological order.

The tracking algorithm begins by reporting to the callback the value of the graph at the starting moment offset by using a query.  However, if a starting graph value was specified to the algorithm and the query returns a value equal to the starting graph value, this initial value is not reported to the callback.

The tracking algorithm then reports all graph nodes in chronological order that have a moment offset greater than the starting moment offset.  If an ending moment offset was provided, then nodes with a time offset greater than the ending moment offset are not reported.

For controller values, tempo changes, and other such global parameters, the tracking algorithm is used with a starting and ending moment offset matching the event range.

For polyphonic aftertouch, the tracking algorithm is used with a starting moment offset one subquantum past the start of the note, and an ending moment one subquantum before the end of the note.  (No aftertouch is used for notes only a single subquantum in duration.)  The note-on velocity of the note is used as the starting graph value in the tracking algorithm in this case.

The tracking algorithm is also used for derived regions when generating graphs, as explained earlier in this document.
