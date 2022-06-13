HM Type Inferencing in C, translated from
[Robert Smallshire's](https://web.archive.org/web/20170109072845/http://smallshire.org.uk/sufficientlysmall/2010/04/11/a-hindley-milner-type-inference-implementation-in-python/)
Python implementation.

The translation is largely straight forward, and contains minimal optimizations
outside of tagged unions because I'm now spoiled by sum types. It has a
hard-coded limit on the maximum number of variables in the same context, which
is currently 20. It also expects callers to pre-allocate the buffer of types to
use, and will return an error if the buffer is too small.

That said, the implementation is a little clever about managing env and
non\_generic, see the above link for more details, which track variables in
scope and whether or not they're generic respectively. The Python implementation
uses a dict which is deep copied repeatedly, this implementation uses a linked
list, and additions (max one per function call) are stored on the local stack,
pointing backwards to the caller's stack recursively. This effectively gives
"structural sharing", lowering memory usage and also eliminating the need for
malloc() and free().

The entire inferencing, in fact, does not perform _any_ memory allocation. The
only allocation done is for printing results. It does, however, expect callers
to pre-allocate the buffer of types to use, and will return an error if the
buffer is determined to be too small. Error types are preallocated before
inferencing so we can handle this case elegantly.

The upshot of all this is that the C implementation will typecheck the
fibonnaci example program in <1 nanosecond (M1 Macbook Air, my laptop). The
Python implementation takes approximately 50ns for this same program. Also,
ironically, the C implementation is slightly smaller ignoring the usage and
printing, even accounting for Python implementation's extra comments.
