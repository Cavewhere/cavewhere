# UASTC kernel on `cwConcurrent::map`

Follow-up to #711 F4 (a7e752f3a "Run the UASTC kernel on the task pool"). Written 2026-10-26. Branch `rebase_dev`.

## 1. The defect

`encodeLevelsOnPool` in `cavewherelib/src/cwKtx2Codec.cpp:311` encodes a texture's mip chain by firing one `cwConcurrent::run` per 256-block chunk, discarding every returned `QFuture`, and waiting on a raw `QSemaphore` that each chunk releases. It runs on the encode lane (a one-thread `QThreadPool` outside the task pool, `cw::ktx2::encodeLane()`, `:399`), reached through `runOnLane` (`:178`) from `encodeRgba` (`:494`).

**Why it is wrong.** Every chunk is enqueued before any runs. A 4k texture is about 17,900 chunks, all at the same priority in the task pool's single FIFO. Anything enqueued after that waits for the whole drain, about 7 s on a 16-thread pool. Nothing can cancel the chunks (F1's promise polls never reach them, so a superseded note still encodes every chunk), no result or error travels through a future, and each `QtConcurrent::run` allocates a runnable and a `QFutureInterface`, tens of thousands per encode.

**How it showed up.** After the rebase, the Catch2 case "Each LiDAR note reaches the render items as it finishes" (`testcases/test_cwNoteLiDARManager.cpp:497`, tag `[cwNoteLiDARManager]`) fails on every run at `CHECK(sawVisibleMidBatch)` and passes on the commit before a7e752f3a (bisected 2026-10-26). A traced run showed the sequence: both notes are admitted together; the lane finishes note 1's encode and immediately queues note 2's 17,900 chunks; note 1 is delivered about 90 ms later, which arms its pick gate and enqueues the intersecter's index build (`cwGeometryItersecter.cpp:2573`) behind all of them; the build starts 5 ms before the last chunk and note 1's gate opens 2 ms after the batch completes. Before a7e752f3a libktx ran the kernel on its own threads, so the pool queue stayed empty.

**What is already right and stays.** The single lane handoff in `runOnLane` is a deliberate semaphore (its comment at `:173`: waiting on a `QtConcurrent` future steals the queued runnable onto the calling thread, which would put several encodes on the cores at once). The release/reserve pair around that wait, `EncodeClaim` (`:341`), the cache path, and the libktx backend selected by `UastcEncoder::Libktx` are unchanged.

## 2. Decision

Replace the fan-out loop with one `cwConcurrent::map` over the chunk indexes and wait on its single future from the lane. Queue ordering by priority was considered and rejected: it reorders the flood instead of removing it, leaves every other same-priority job able to starve its neighbors, and adds an ordering rule every pool user would have to know.

**Why map fixes it.** `cwConcurrent::map` (`cavewherelib/src/cwConcurrent.h:61`) is `QtConcurrent::map` on the task pool with each item tagged as a task interval. Qt's map engine is a pull model: it starts at most pool-thread-count workers and each one iterates the sequence, so the queue never holds more than that many items. A job that arrives behind an encode waits at most one chunk, about 6 ms. Throughput on an idle machine is unchanged because every pool thread stays busy until the sequence is exhausted. The engine returns one `QFuture<void>`, which the lane can wait on and which is cancelable, the hook a later F1 wiring needs. The codebase already uses this shape in eight places (for example the intersecter's build maps at `cwGeometryItersecter.cpp:2320` and `:2361`).

## 3. The item

### K1. Encode UASTC chunks through one map (edit)

**Goal.** An encode never holds more pool queue entries than the pool has threads, every chunk's work is carried by one future the lane waits on, and the mid-batch pick gate opens again for the LiDAR case.

**Files.** `cavewherelib/src/cwKtx2Codec.cpp` (`encodeLevelsOnPool :311`, `runOnLane :178`, `encodeRgba :494`, constants `:60-70`), `testcases/test_cwKtx2Codec.cpp`, `testcases/test_cwNoteLiDARManager.cpp` (existing case, unchanged). No public API changes. `cwConcurrent.h` only if step 2 needs a blocking variant (see pitfalls).

**Steps.**

1. Build a flat chunk list once per encode: a small struct `{const UastcLevel* level; int first; int last;}` (or level index plus block range) in a `QList` or `std::vector`, one entry per `kBlocksPerChunk` run of every level, in the same order the loop produces today. Reserve it from the total block count so the build does no reallocation.
2. Replace the loop and the semaphore with `cwConcurrent::map(chunks, [quality](const Chunk& chunk) { encodeBlockRun(*chunk.level, chunk.first, chunk.last, quality); })` and `future.waitForFinished()` on the lane. This is `QFuture::waitForFinished`, a plain blocking wait on a thread outside the pool and the event loop, which the project rule about `AsyncFuture::waitForFinished` (test-only, spins a nested loop) does not cover. Keep the `pool == nullptr` fallback that encodes inline.
3. Keep `kBlocksPerChunk = 256` as the item size so the per-item task interval that `batchFunctor` opens stays about 6 ms of work, which is also what the trace views want. Delete `chunksFinished` and `chunkCount`. Update the doc comment above `encodeLevelsOnPool` to say the map keeps the queue to the pool's width.
4. Measure (§4) and record the result in §7.

**Tests** (`testcases/test_cwKtx2Codec.cpp`, tags `[Ktx2][EncodeLane]`).

- **New, fails today:** "A pool item queued during an encode starts within one chunk". Start an encode of an image large enough for a few thousand chunks (the existing helpers `startEncode` / `gradientImage`; pick a size whose chunk count is a named constant), wait until the pool is busy with it (spin until a probe item queued before the encode has finished, or until the encode's task-pool threads are active), then queue a trivial `cwConcurrent::run` and assert it finishes well before the encode does: `spinUntil(trivial.isFinished, kChunkWaitBudgetMs)` with the budget a named constant well above one chunk and well below the encode, and `CHECK_FALSE(encode.isFinished())` at that moment. Today the trivial item waits for the entire drain, so the check fails.
- **Existing, must keep passing:** "Both UASTC encoders produce the same KTX2 bytes" (byte-exact against libktx, `:782`); "The task-pool encoder honors a one-thread pool" (`:810`, both sections: the map engine with one pool thread still runs every chunk); "A worker waiting on the encode lane frees its pool slot" (`:644`); "The task-pool encoder is the default" (`:838`; with every pool thread held the encode must still make no progress, which holds as long as the lane waits on the map rather than running its items itself, see pitfalls); "A one-thread pool encodes the same pixels as the default pool" (`:672`); and the LiDAR case "Each LiDAR note reaches the render items as it finishes" (`[cwNoteLiDARManager]`), which is the regression's own test.

**Gate.** `cavewhere-test "[Ktx2],[Ktx2Codec],[EncodeLane],[cwNoteLiDARManager]"`. Run the LiDAR case three times in a row; it failed 3 of 3 before.

## 4. Measurement and pass line

**Measure.** `scripts/profile_tasks.py` on the large-scrap project, cold, A-only, three runs, compared with the F4 point it was recorded against (`build/profile/tasks/f4k-pool14`). The runner needs a `CAVEWHERE_TASK_TRACE=ON` Release build (`build/Qt_6_11_2_for_macOS-Release`). Prune the xctrace scratch in `$TMPDIR` after each point.

**Pass.**

- The LiDAR mid-batch case passes 3 of 3 and the new chunk-wait test passes.
- `ktx2/cpu_s` and time to idle within the F4 point's spread (the kernel does the same work on the same threads; only the queue shape changes).
- `threads/pool/created` no higher than the F4 point (a map engine starts at most pool-width workers; the old loop's runnables created no threads either, so this is a check, not a target).
- Peak pool queue depth during an encode at most the pool's thread count, read from the trace if the runner exposes it, else from the chunk-wait test alone.

## 5. Pitfalls

- **Blocking map vs map.** `QtConcurrent::blockingMap` would let the lane thread run items itself, which is faster on paper but changes the test "The task-pool encoder is the default" (an encode would progress with every pool thread held) and puts a 17th encoder on the cores. Use `cwConcurrent::map` plus `waitForFinished`; if Qt's wait turns out to steal the engine onto the lane (the Qt 6.11 engine sets no runnable, so it should not), note it in the result and keep the behavior the tests pin.
- **Item lifetime.** The chunk list and the levels it points into must outlive the wait; they are locals of `encodeLevelsOnPool` and the wait is in the same frame, so a plain container is enough. Do not capture `UastcLevel` by value per item: `blocks` is a raw pointer taken once so the chunks write disjoint ranges without detaching (`:488`).
- **Per-item overhead.** `batchFunctor` opens a task interval per item; at 256 blocks per item that is the same count of intervals as today's runs. Mapping single blocks would be 16× more intervals; keep the chunk.
- **Pool width of one.** The map engine with `maxThreadCount() == 1` runs one worker; the caller's slot was already released in `runOnLane`, so the one worker is free. The one-thread tests cover it.
- **Cancellation is a follow-up.** The map future supports `cancel()`, but `ensureEncodedEntry` has no promise to read. Wiring F1's cancel through the lane to the map future is recorded under follow-ups, not done here.
- No magic numbers (the chunk size and test sizes are named constants), `.at()` for reads, American English, positive phrasing in comments.

## 6. Follow-ups (recorded, not in K1)

- Cancel: pass a cancel token or the caller's promise through `ensureEncodedEntry` so a superseded note's encode cancels its map; the lane then skips the cache insert.
- The codebase audit for the same pattern (semaphores, discarded futures, direct `QThreadPool::start`, unbounded fan-out) is in progress; its findings get their own plan items.

## 7. Status

| Step | State |
|---|---|
| Bisect and trace | done 2026-10-26: a7e752f3a is the first failing commit; FIFO drain of 17,900 chunks confirmed by trace |
| K1 | not started |
| Measurement | after K1 |
