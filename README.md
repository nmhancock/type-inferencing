HM Type Inferencing in C, translated from
[Robert Smallshire's](https://web.archive.org/web/20170109072845/http://smallshire.org.uk/sufficientlysmall/2010/04/11/a-hindley-milner-type-inference-implementation-in-python/)
Python implementation.

This implementation has hard-coded limits and uses a single global collection
for all types found. This can be improved by making the inferencing_ctx struct
an argument to the call stack of analyze(), I didn't bother.

That said, the implementation is a little clever about managing env and
non\_generic, see the above link for more details, which track variables in
scope and whether or not they're generic respectively. The Python implementation
uses a dict which is deep copied repeatedly, this implementation uses a linked
list, and additions (max one per function call) are stored on the local stack,
pointing backwards to the caller's stack recursively. This effectively gives
"structural sharing", lowering memory usage and also eliminating the need for
malloc() and free().

The entire inferencing, in fact, does not perform _any_ memory allocation. The
only allocation done is for printing results.

The upshot of all this is that the C implementation will typecheck the
fibonnaci example program in <1 nanosecond. The Python implementation takes
approximately 50ns for this same program. Also, ironically, the C implementation
is slightly smaller ignoring the usage and printing, even accounting for
comments.
