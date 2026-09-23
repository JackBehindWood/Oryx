# Memory Allocator

Status: **decided and built** (6a–6c.3): the module, own smart pointers, the pool as the default with every memory reporter on its counts, and the containers on `IAllocator*`.

## Why

Oryx allocates its own objects through the global `operator new` today. Three things follow from that:

* **Measurement is expensive.** The allocation census (`MemoryTracker`) has to ask the platform allocator how big every freed block is (`malloc_size`), because an unsized `delete` doesn't say. On the Python `decide` path that lookup was measured at about 77 ns of 587 ns.
* **Measurement is missing in the research host.** The census replaces `operator new`, which only a program may do. The `oryx` Python extension therefore has no memory numbers (`benchmark(memory=True)` raises).
* **There is no seam for special-purpose memory.** Search algorithms that allocate per node (MCTS, cloned states) will want an arena that `undo` can rewind.

The fix is an Oryx-owned allocator module, `Oryx/src/Oryx/Memory/`. A size-class pool becomes the default source for Oryx objects, and every Oryx pointer remembers the allocator that made it, so it can free the exact size back to it.

## Scope

In scope: objects Oryx creates through `create_unique`/`create_shared`, the `SmallVector`/`FlatHashMap` containers, and the Python adapters (through the same functions).

Out of scope: third-party allocations (libc++ containers such as `std::vector`/`std::string`, pybind11, spdlog, yaml-cpp). They keep the system heap. The global census stays as the whole-program number.

## Building blocks

| Type | Role |
|---|---|
| `IAllocator` | Pure-virtual interface: `void* allocate(size_t size, size_t alignment)`, `void deallocate(void* pointer, size_t size, size_t alignment)`. Every free is sized; there are no usable-size lookups. |
| `HeapAllocator` | Upstream allocator over the global `operator new`/`delete` (aligned forms when needed), so the census still sees the traffic in executables. |
| `PoolAllocator` | Size classes of fixed-size blocks carved from chunks, with intrusive free lists. |
| `CountingAllocator` | Wraps any `IAllocator` and counts into a `MemoryStats` with relaxed atomics. Exact, because every call is sized. |
| `ArenaAllocator` | Bump pointer with `reset()` and `mark()`/`rewind(mark)`; `deallocate` is a no-op. |
| `default_allocator()` | Returns the process-wide default stack: `CountingAllocator` over `PoolAllocator`. |
| `default_allocator_stats()` | The default stack's `MemoryStats`; what benchmarks read. |

A virtual interface rather than a template policy: allocators are chosen at run time (default vs arena), stored in pointers and containers, and a virtual call is cheap next to the work it saves. The allocator is passed explicitly; there is no thread-local "current allocator".

## Decisions

### Default allocator lifetime

`default_allocator()` placement-constructs the stack into static aligned storage on first use and never destroys it.

* Static-initialisation self-registrations (`OX_REGISTER_*`) allocate before `main`, so the default must exist on first call. A function-local static gives that, and its initialisation is thread-safe.
* Objects freed during static destruction (registries, settings sections) still need a live allocator, so it is never destroyed. Its chunks are reclaimed by the operating system at exit.

### Thread model

One global pool with **one spinlock per size class** (`std::atomic_flag`, test-and-test-and-set).

* The pool is not single-threaded: batches run with the GIL released, Python threads can create objects, and static initialisation allocates.
* A lock per size class is the smallest correct design. Contention is low because most hot paths allocate from one thread at a time.
* Thread-local caches are added only if a measurement shows lock cost on `decide` or in batches.

### Pool shape

* Size classes from 16 to 512 bytes: powers of two plus their midpoints (16, 24, 32, 48, 64, 96, 128, 192, 256, 384, 512). A request goes to the smallest class that fits it **and whose size is a multiple of the requested alignment**, so a 16-aligned 20-byte request uses 32, not 24. Blocks of a class then sit at multiples of the class size from a 16-aligned chunk base, and every block is aligned.
* 64 KiB chunks from `HeapAllocator`, split into blocks of one class on demand. Chunks are never returned to the heap.
* Anything larger than 512 bytes, or aligned beyond 16 bytes, goes straight to `HeapAllocator`; the counting layer above the pool still counts it like any other block.

### Counting

The default stack is `CountingAllocator(PoolAllocator)`, so `default_allocator_stats()` counts Oryx objects exactly. The byte counts are the sizes asked of the allocator: for a smart-pointer object that is the object plus its record (below), so they are slightly larger than `sizeof` of the object. Numbers recorded before this change, such as `bench_minimax_allocation`'s 176 bytes, will shift for that reason alone. The counting layer is separate so any allocator (an arena, a test allocator) can be measured the same way. The counters are folded into the pool only if the extra virtual call shows up in a measurement.

### `UniquePtr<T>`

Oryx's own type replaces the `std::unique_ptr` alias in `Core/Base.h`. The name stays, and `create_unique<T>(args...)` keeps its signature (it uses the default allocator). `allocate_unique<T>(allocator, args...)` takes an explicit one (a separate name, not an overload, so a constructor whose first parameter is an allocator is never ambiguous; `allocate_shared` likewise).

Layout: 16 bytes, `{ T* object; AllocationRecord* record; }` (`std::unique_ptr` is 8, so each owning member grows by 8 bytes).

```text
one block from the allocator:
[ AllocationRecord | padding | Concrete object ]
  allocator*            (to the object's alignment)
  block size, alignment
  strong count          (unused while the object is uniquely owned)
  destroy(record*) thunk, instantiated for Concrete
```

`UniquePtr` and `SharedPtr` share **one record type**, so moving a `UniquePtr` into a `SharedPtr` hands the record over with no new allocation. The Python backend relies on that: `SharedPtr<IGame> game = create_game(name)` in `PyResolve.cpp`.

* `create_unique<Concrete>` allocates the record and the object in **one** block. The destroy thunk is instantiated for the concrete type, so destroying through a `UniquePtr<IState>` runs the right destructor and frees the exact block size. No virtual destructor lookup is needed to find the size.
* If `Concrete`'s constructor throws, `create_unique` returns the block to the allocator before the exception propagates.
* `UniquePtr<T>` never needs `T` to be complete except in `create_unique`, because destruction goes through the record. The pimpls in `Settings` and `Random` (`UniquePtr<Impl>` over an incomplete `Impl`) keep working without out-of-line destructors.
* Converting `UniquePtr<Derived>` to `UniquePtr<Base>` copies the record pointer and lets the compiler adjust the object pointer, so multiple inheritance works.
* Oryx code never adopts raw pointers. The one current case, `PythonContext::current()` resetting from `new PythonContext()`, becomes `create_unique` (the private constructor gets a factory). The single exception is the pybind11 holder, below.
* `release()` has 5 uses today, all on `PyRef`/`py::object`, which are not `UniquePtr`, so the type does not offer it.

### `SharedPtr<T>`

The same single block, with the record extended by an atomic strong count.

* Atomic, because handles are copied under the GIL but batches run with it released.
* No weak count: nothing in the codebase uses `weak_ptr` or `shared_from_this`.
* `SharedPtr<void>` (settings sections) and `SharedPtr<const T>` (script origins) are supported, since the destroy thunk and size live in the record, not in the pointer's type.
* Size: 16 bytes, the same as `std::shared_ptr`.

### pybind11 holders

The three Python classes held by `SharedPtr` (`StateHandle`, `ActionFeatures`, `Context`) declare `oryx::SharedPtr` as a holder with `PYBIND11_DECLARE_HOLDER_TYPE`. `py::cast(create_shared<PyContext>(...))` casts an existing holder, which copies it and bumps the strong count, as `std::shared_ptr` does today.

pybind11 also needs `holder_type(T*)` to compile: it adopts a raw pointer when it owns an instance that arrived without a holder (`pybind11.h`, `init_holder`). `SharedPtr` therefore has one **explicit** adopting constructor. It allocates a separate record from the default allocator whose destroy thunk calls `delete`, because pybind11 made that object with `new`. None of the three classes has a Python constructor or is returned by value, so the path shouldn't run today. A test covers it anyway, because it is reachable in principle.

### Containers

`SmallVector` and `FlatHashMap` hold an `IAllocator*` that defaults to `default_allocator()`. It replaces their direct `::operator new`/`delete` and `new bool[]`.

* The allocator is resolved on the first spill, so a container that stays inline never touches it.
* A copy takes the source's allocator. A move hands over the buffer together with the allocator that owns it, so a buffer is always freed where it came from.
* Cost: 8 more bytes per container.

### Arena

`ArenaAllocator` is never a default. It exists for code that knows its lifetimes, first search: `mark()` before `apply`, `rewind()` after `undo`. `deallocate` is a no-op, so smart pointers into an arena cost nothing to free. Rewinding does **not** run destructors, though, so every pointer into the rewound region must be destroyed before `rewind()`. Poisoning (below) makes ASan report a violation.

### AddressSanitizer

A pool hides use-after-free from ASan, because freed blocks stay inside a live chunk. The pool therefore poisons a new chunk's unused remainder, poisons a block when it is freed (after writing the free-list link) and unpoisons it when it is handed out. The arena poisons on `rewind`/`reset`. `OX_ASAN_POISON`/`OX_ASAN_UNPOISON` compile to nothing without ASan (`__has_feature(address_sanitizer)` / `__SANITIZE_ADDRESS__`).

### Benchmark semantics

`MemoryStats` stays the report type and gains a documented **source**:

| Source | What it counts | Where it works |
|---|---|---|
| `default_allocator_stats()` | Oryx objects, exact sizes asked of the allocator (object plus record) | every host, including the Python extension |
| `MemoryTracker` (census) | the whole program: third-party allocations plus the pool's chunks | executables only (Oasis, Tests) |

`MemoryBenchmarkRunner`, `SimulationLayer`'s memory results, `oryx.benchmark(memory=True)` and `bench_minimax_allocation` switch to the pool's numbers **in the same step** that makes the pool the default. Otherwise their results would silently change meaning: the census would only see chunk traffic.

### What stays `std`

* spdlog's API takes `std::shared_ptr` (`Log.h`, `TestLogCapture.h`).
* `std::function` factories in `Registry<T>`: the factory's return type changes with the alias, but the `std::function` itself is a value and is unaffected.
* libc++ containers keep their allocator. Moving them onto the pool is not planned.

## Rollout

Each step ends with every build leg green (debug, release, dist, `--no-python`, `--sanitize --profile release`) and, from 6c on, a benchmark gate:

1. **6b — the module alone.** `Oryx/src/Oryx/Memory/`: `IAllocator.h`, `HeapAllocator`, `CountingAllocator`, `PoolAllocator`, `ArenaAllocator`, `DefaultAllocator`, `AsanPoison.h`. Tests cover alignment, size-class routing, the large fallback, count balance, arena mark/rewind, concurrent allocation from several threads, and an ASan use-after-free check under `--sanitize`. Nothing uses the module yet.
2. **6c.1 — own smart pointers on the heap.** `UniquePtr`/`SharedPtr`/`create_*` replace the `std` aliases, still backed by `HeapAllocator`, plus the pybind11 holder. This measures the pointer swap on its own.
3. **6c.2 — the pool becomes the default**, and in the same step every memory reporter reads `default_allocator_stats()`. `benchmark(memory=True)` stops raising in the Python extension.
4. **6c.3 — containers on `IAllocator*`.**

**Benchmark gate** for each 6c step: `decide`, the Python adapter calls, `bench_minimax_allocation` and the end-to-end batches, measured as an **interleaved A/B** against a saved binary of the previous step (samples taken minutes apart drift by several nanoseconds). `decide` must not get slower than the previous step (587 ns Release at the start). The target is the ~510 ns measured before the census began sizing every free. A regression is fixed, or the switch is reverted in favour of thread-local caches or counters fused into the pool.
