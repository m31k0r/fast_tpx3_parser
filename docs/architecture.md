# Architecture

## Scope and status

The repository contains the building blocks of a TPX3 parser and a concurrent
processing pipeline. CMake currently builds a library, unit tests, and
benchmarks; it does not define a user-facing parser executable. The
`src/main.cpp` file is not part of a CMake target, and portions of the proposed
consumer pipeline are not implemented as buildable components. Treat the
pipeline below as a description of available components, not a promise of a
complete end-to-end application.

## Data flow

```text
TPX3 file
    │
    ▼
MemoryMap ──► Parser ──► per-chip hit queues (4)
                         trigger queue
                              │
                              ▼
                      Sort / SortingBuffer
                              │
                              ▼
                       optional Merger
                              │
                              ▼
                     queue-draining Sink
```

The sorter and merger are reusable components, but the CMake-built project does
not connect them into a runnable application. `Sink` only removes batches from
a queue; it does not decode or persist them.

## Components

### File mapping and chunk parsing

- `MemoryMap` maps a file into memory and exposes its contents as 64-bit words.
  The Linux implementation uses `mmap`; platform-specific mapping code is
  declared in `Memory_Map.hpp`.
- `tpx3_helper::parse_header` validates the `TPX3` marker in the first four
  bytes of a word, reads the chip number and chunk-size field, and returns the
  word range associated with that chunk.
- `Parser::parse` walks the mapped words. It classifies each word by its high
  nibble: hit (`0xb`) words go to a chip-specific queue, trigger (`0x6`) words
  go to the trigger queue, and frame (`0x7`) words are currently ignored.
- The parser queues the original encoded 64-bit words. The `Hit` and `Trigger`
  helper types provide field/time conversions, but are not used to decode
  records in the parser-to-queue path.

### Batches and ownership

`Batch<T>` stores up to `BATCH_SIZE` entries (currently 131,072) and tracks its
insert position and sequence number. `Batch_Pool` preallocates batch objects
and returns them in `unique_ptr`s whose deleters return the objects to the
pool. This lets queues transfer batch ownership without allocating a batch for
each enqueue.

`BatchHandle` owns the pool, four hit queues, one trigger queue, and the
currently accumulating batches. When a batch fills, it is submitted to its
queue and replaced with a batch from the pool. Queue capacity and pool size are
defined in `src/Constants.hpp`.

### Sorting and merging

- `SortingBuffer` is a fixed-capacity ordered window. `Sort` consumes input
  batches, inserts their words into that window, and emits words that leave the
  window in output batches. `Compare_Hits` and `Compare_Triggers` extract the
  encoded time fields used for ordering.
- Each emitted batch has a sequence number. `Merger` selects batches by
  sequence number across input queues and sorts a small boundary region between
  neighboring batches.
- These components are independently tested, but the merger is not currently
  wired into the benchmark pipeline or a CMake-built application.

### Other utilities

- `Rotation` maps detector coordinates for the eight orientation strings
  recognized by `Rotation::from_string`.
- `Queue_State_Printer` periodically reports queue occupancy.
- The bundled MPMC and SPSC queue implementations provide bounded concurrent
  queues used by the pipeline.

## Current limitations

- There is no configured CLI/application target or documented supported
  command-line interface.
- The benchmark expects a local `tpx3.tpx3` file; no sample input is checked in.
- A final partially filled parser batch is not explicitly flushed by
  `Parser::parse`.
- Frame words are recognized by their nibble but are not processed in the
  parser.
- The architecture describes the current source layout; it should not be read
  as a guarantee of a production-ready, fully connected pipeline.

## Source map

| Path | Responsibility |
| --- | --- |
| `src/Parser.*` | File traversal and dispatch of encoded words |
| `src/Memory_Map.*` | Memory-mapped input |
| `src/tpx3_helper.*` | TPX3 header, hit, trigger, and frame helpers |
| `../src/Batch/Batch.hpp`, `../src/Batch/Batch_Pool.hpp` | Fixed-size batches and batch reuse |
| `src/Sorter.hpp`, `src/Sorting_Buffer.hpp`, `src/Compare.hpp` | Bounded-window ordering |
| `src/Merging_Buffer.hpp` | Sequence-based batch selection and boundary ordering |
| `src/Sink.*`, `src/QueueConcepts.hpp` | Queue draining and queue interface concepts |
| `src/Rotation.*` | Coordinate orientation transforms |
| `tests/` | Unit tests for batch-pool, sorting/merging, and rotation components |
| `benchmarks/benchmark.cpp` | Parser and parser-plus-sorter benchmarks |
