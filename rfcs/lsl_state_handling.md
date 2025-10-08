# State management logic for LSL scripts

**Status**: Implemented

## Summary

Unlike typical Lua scripts, LSL scripts are conventionally driven by an LLScriptExecute subclass within lscript.

New coroutines may never be spawned by scripts, coroutines are only ever scheduled in response to external events from
the script engine. Event handlers are also never explicitly managed by the script itself. The only mechanism for
swapping out event handlers is through the `state` statement in the LSL grammar. This document discusses the
semantics of state switching, and how that should be handled by embedders, as well as the semantics of LSL mechanisms
like forced sleeps.

Similarly, LSL event handler execution may be interrupted for a number of reasons. One is the state may be changed
in the middle of an event handler. Another is an out-of-memory may occur. Another is that a script may have overrun
its time budget and may be pre-emptively yielded by an interrupt handler. We need some way to distinguish between
all of these different events and handle them appropriately.

## Design

```
┌──────────────────┐                                            ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐
│Grandparent Thread│◀─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ Has only default globals  
└──────────────────┘                                            └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘
          │                                                                                
          │                                                ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ 
          │    ┌─────────────────┐                          Owns script-specific globals, │
          ├───▶│Base Image Thread│◀ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─│ executes once to initialize   
          │    └─────────────────┘                          ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘
          │                                                ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ 
          │                                                   Spawns children from base   │
          │             ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─│            image              
          │            ▼                                    ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘
          │    ┌───────────────┐                                                           
          ├───▶│ Forker Thread │─┐                               ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐ 
          │    └───────────────┘ │  ┌────────────────────┐        byte-serialized initial  
          │                      └─▶│Initial Thread Image│◀ ─ ─ ─│         state         │ 
          │                         └────────────────────┘        ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─  
          │                                                       ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐
          │            ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─    Spawned by forker    
          │                                                       └ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┘
          │            ▼                                                                   
          │    ┌───────────────┐                                  ┌ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ┐
          ├───▶│ Child1 Thread─┼┐   ┌────────────────────┐           Currently executing   
          │    └───────────────┘└──▶│ state_entry thread │◀─ ─ ─ ─│   handler (if any)    │
          │                         └────────────────────┘         ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ ─ 
          │    ┌───────────────┐                                                           
          ├───▶│ Child2 Thread │─┐  ┌────────────────────┐                                 
          │    └───────────────┘ └─▶│ touch_start thread │                                 
          │                         └────────────────────┘                                 
          │    ┌───────────────┐                                                           
          ├───▶│ Child3 Thread │                                                           
          │    └───────────────┘                                                           
          │                                                                                
          │    ┌───────────────┐                                                           
          └───▶│Child... Thread│                                                           
               └───────────────┘                                                                                                                    
```

### "Threads"

Above is the typical layout of a ServerLua VM state. Lua "Thread"s are a bit of misnomer, they're sort
of like nested states which can hold data on the stack (potentially) have their own globals table, and
can run coroutines.

Each of the Child threads are spawned by the Forker thread using the Base Image thread as a template.
The Base Image thread owns the underlying code for all the functions and handlers declared in the code,
and coroutine wrappers around that code are created in each Child thread, making them relatively lightweight.
Each Child Thread has its own globals table, making its mutations to globals invisible to its siblings.

When executing an event handler within a Child Thread, a new Thread is pushed onto the Child Thread's
stack, and a coroutine for the corresponding event handler is executed within that handler thread.
This is necessary so that handler execution can be cancelled without tearing down the entire child script,
as `state` switching would require.

No code is generally run directly on the Child Threads, they exist only to keep global state for a script
alive between invocations of handlers. The only exception is that whenever a script is restarted,
it _must_ run the main function within the Child VM. This happens _before_ the default `state_entry()` is
executed, and can be thought of as running the "constructor" for the script instance. This "constructor" should
be run synchronously, and should not yield. The "constructor" is responsible for making concrete closure instances
based on the function prototypes present in the bytecode, as well as assigning the initial values to all globals.

### State serialization

Serializing the runtime state of a Child thread is simple. Essentially, the runtime state is diffed against the state
of the base image using Ares. The only thing stored in the serialized version is the difference between that base
image and the current script (generally just the globals and the stack.) This ends up being roughly similar to
how the Mono state serialization currently works.

### `state` handling

Unlike Lua scripts, LSL scripts have a notion of `state`. This requires a little cooperation from the script
engine host. Traditionally, this is tracked through a "next state" and "current state" register, where the
`state` statement changes the "next state" register, and a mismatch between "current state" and "next state"
triggers scheduling of the current state's `state_exit()` before triggering the next state's `state_entry()`.

Mono internally throws an `Exception` to trigger stack unwinding and set the "next state" register. Under
the Luau VM, we can emulate this by making the script `yield` an integer indicating the state to switch to.
Because yields injected by the scheduler do not include a value, we may distinguish between yields due to
state switching vs yields due to interruption based on how many values were yielded.

Unlike Mono, which stores data relevant to the script host (like next / current state) on the `LSLUserScript`
object instance, under Luau it is the script host's duty to serialize this separately, and this data is never
stored within the VM. This is done by separately serializing an LLSD payload alongside the script state which
contains the necessary data. This ensures that data which is only relevant for the script host doesn't count
against the script's memory usage, and doesn't require messing with VM internals.


## References

* https://github.com/secondlife/SLua/blob/main/VM/src/ares.cpp
* https://github.com/secondlife/server/blob/4bdd846e1a7afcc04c6867257534cde6c576dffa/indra/lib/mono/indra/LslUserScript.cs#L49-L104
