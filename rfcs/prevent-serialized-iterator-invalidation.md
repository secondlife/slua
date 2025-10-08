# Prevent invalidation of Table iterators when serializing Threads

**Status**: Implemented

## Summary

Implement stable iteration order for deserialized tables to ensure iterators
aren't invalidated.

## Motivation

Serialization for whole-VM execution state is supported by Ares. As part of
that, we must be much more careful about iteration order for deserialized
tables than Lua would generally need to be. While it would normally be fine
for a table that technically has the same contents as another table to have
a different iteration order, from the point of view of code running in a
just-deserialized VM, this is not a different table, so we can't diverge from
the iteration order the table was using at the point it was serialized.

Without preserving iteration order, it becomes unsafe to ever serialize a
VM while a table is being iterated over without making breaking changes
to Lua's iteration semantics. Without that, the code might loop over a
key twice or skip a key entirely once the VM is revived.

This is further complicated by the fact that the hash portion of tables have
no natural, specified iteration order, and that keys of tables may be hashed
by their pointer, which will be different when the script reloads.

This change aims to preserve the original iteration order for deserialized
tables until a key is either added to or removed, ensuring that iteration
semantics are preserved across script load / unloads. Making tables in
general iterate in a particular order (in insertion order, for instance) is a
non-goal.

## Design

We need to account for two different ways that Luau represents iterator
state:

* `TValue` with the value of the last key iterated over, used to pass into
  `next(table, key)` or the C++ API equivalent.
* In the case of Luau's specialized `FORGLOOP` instructions, native `int`
  indices. These indices are relative to the range `sizearray + sizenode`.
  It must be noted that these are the _allocated_ size of each structure,
  and depending on how the table is constructed, `Table`s with the exact
  same contents may have different `sizearray` and `sizenode`s.

These weren't generally an issues for users of Pluto or Eris under PUC-Rio Lua
because iterator state was usually represented by the key last returned from
`next()`, rather than an opaque integer index in range `sizearray + sizenode`.
Technically iterating over tables with keys that used pointer hashing like
`userdata` or `table` would still break under Pluto or Eris, but was probably
uncommon enough that nobody noticed the breakage.

### Plan

TODO: expand on these when I feel like it.

* Add a new `LuaNodeIterOrder* iterorder` field to `Table` which, if present,
  is used for iterating over nodes rather than `nodes` directly.
* Use the new field on `LuaTable` to specify the node's index within the custom
  iteration order. This allows `lua_next()` to work without requiring exhaustive
  scan of keys to preserve iteration order. Does increase per-node memory usage.
* storing the `nil` holes in internal array / node arrays is now important
  because never inserting vs explicitly inserting nil give different internal
  representations, meaning different indices! forgloop uses internal indices!
* serialize array size and node size, resize arrays before inserting into table
* check that the array size still matches after inserting all items
* * node size may differ due to hash changes for userdata and table keys?
* Once this is done, should only need to store an array of `nodesize` `LuaNodeIterOrder*`s
  that specifies the iterorder of the node portion, pointing at nodes in `node`,
  or at `-1` where we need a `nil` hole.
* * No need for explicit size field on `Table` because size should always be `nodesize`
* Obviously, switching to insertion-order preserving hashmap impl also solves
  this problem, but harder for me to implement + drifts from upstream.
* Everything after first `nil` in array may be in hashmap portion, only keys
  up to `#t` are guaranteed to be iterated in order (though items after `#t`
  may still be in the array)
* If we want to ensure iteration order, we have to be sure that things that
  came from the hash part go back in the hash part even if they could technically
  fit inside the array part. Maybe do the hash insertions first and then alloc
  the array so we can control the size of the array portion.
* Make sure to prevent GC while doing the initial insert into the array so
  the GC doesn't try to "help" by shrinking the array or node portions.
* * RAII thingy to help with this
* Only explicitly adding a new key to a table can cause the node list to
  shrink. I believe this may be so iterating over a weak table doesn't break
  if a node gets killed during iteration.
  Addressed here https://luau-lang.org/performance#reduced-garbage-collector-pauses


## Drawbacks

* There are ABI changes to the `Table` to support the new field, adding at
  least `size_t` bytes to the base `Table` size.
* All assignments to a table now require a check to see if a key is being
  logically added or removed so we can do iteration order invalidation if needed
* All places where `Table.node` is being read directly for iteration need
  to be updated to conditionally use the explicit iteration list

## Alternatives

### Switch to insertion-ordered hashtable and add iterator serialization

Swapping the `Table` implementation with one that preserved insertion order in
the hash portion was considered, and would technically be ideal, but wasn't done
due to the non-trivial amount of work involved.

Switching to an insertion-ordered `Table` would likely involve replacing large
chunks of `ltable.cpp`, and changes to how `Table` compaction and GC work. Not
even getting into the fact that I'm not 100% sure how to implement such a thing,
making large changes to the `Table` implementation that are likely to be
upstreamed into Luau would make merges very annoying to deal with. By contrast,
the API changes for a secondary explicit iteration list result in merge conflicts
that are relatively easy to spot and deal with.

Depending on the insertion-ordered hashtable impl used, we may also have to add
special iterator (de)serialization as below unless we were very careful about
maintaining tombstones and such.

Changing `FORGLOOP` to use a different iterator representation than an index into
the range `table->array + table->node` might allow serializing iteration state in
a `table->array + table->node` size-independent fashion.

Since the iterator is actually pushed as a `lightuserdata` `TValue`, we could abuse
the `TValue.extra` field on every `TValue` (but unused for `lightuserdata`) to store
some extra context about the iterator. When serializing and deserializing, we could
look at the `TValue.extra` field of `lightuserdata`s to determine if they're iterators,
and what `Table` they are iterators for. That would allow us to convert them to
serialization-stable key forms that referenced the actual key object, and could be
deserialized to their integer index within the deserialized `Table`'s hash portion.

Some caveats to this approach are that it would be complicated to implement, and would
require making `TValue.extra` semantically significant for `lightuserdata`s. Otherwise
we would have to change the stack effects of `FORGLOOP` and friends to include additional
context, which would also require changing the JIT codegen.

Additionally, maintaining insertion order for mixed tables or arrays with holes would
be... weird. Things being in the array or node part of a table are meant to be an
implementation detail, but it becomes _not_ an implementation detail if you want to
guarantee you'll always preserve insertion order across mutations. I suppose you could
use an insertion ordered hashtable even without explicitly making that guarantee, but
I see no possibility for upstreaming that.

Thinking about it makes my head hurt, and I don't want to write it, even if it's technically
better.

## References

* https://luau-lang.org/syntax#generalized-iteration
* https://luau-lang.org/performance#reduced-garbage-collector-pauses (bottom paragraph)

Insertion order-preserving hashtable implementations:

* https://www.pypy.org/posts/2015/01/faster-more-memory-efficient-and-more-4096950404745375390.html
* https://www.npopov.com/2014/12/22/PHPs-new-hashtable-implementation.html
* https://bugs.python.org/issue27350
