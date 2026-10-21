# Point octree pack file

One cache file per LAZ tile replaces the one-file-per-node layout the point
octree builder writes into `.cw_cache`. The renderer opens the pack once per
cloud, maps it, and reads node payloads out of the mapping. This plan is written
for implementation by a workflow; every section states what to build, what to
delete, and what the tests have to prove.

## 1. Why

On iOS every `QFile`, `QFileInfo`, and `QDir` consults Qt's own
`SecurityScopedFileEngineHandler` (Qt 6.11, qtbase
`src/corelib/platform/darwin/qdarwinsecurityscopedfileengine.mm`). Its
`create()` runs `realpath` on the path before it decides anything, about 1 ms
per call, and there is no opt-out. After commits `777ad360` and `0664175c` the
streamer's per-node load is down to one `QFile::open`, which is still 140 ms of
a 157 ms worker sample (the actual `read` is 7 ms). `cachedManifest()` also
stats every node of every tile at project open, thousands of `realpath` calls
per tile.

A LAZ tile is added or removed as a whole, so one file per tile loses nothing.
One hundred open tiles is one hundred mappings.

## 2. Design

### 2.1 New class `cwPointOctreePack`

Files: `cavewherelib/src/cwPointOctreePack.h`, `cavewherelib/src/cwPointOctreePack.cpp`
(picked up by the CMake glob). Export with `CAVEWHERE_LIB_EXPORT`. Its own
class, separate from `cwDiskCacher`, which keeps serving images, textures, and
glTF.

Two parts:

**`cwPointOctreePack` (reader).** Immutable after `open()`, so any number of
worker threads read it without locking.

```cpp
class CAVEWHERE_LIB_EXPORT cwPointOctreePack
{
public:
    //! Opens, maps, and validates the pack at packPath. nullptr when the file
    //! is missing, truncated, from another format version, or its index and
    //! manifest fail the checksum. The one QFile::open per cloud happens here.
    static std::shared_ptr<const cwPointOctreePack> open(const QString& packPath);

    const cwPointOctreeManifest& manifest() const;

    //! A copy of node index's payload bytes (manifest node order), empty when
    //! the index is out of range. The copy is a memcpy of at most ~160 KB; the
    //! mapping stays owned by the pack so callers never hold a raw pointer
    //! into it.
    QByteArray node(int index) const;

    int nodeCount() const;
    QString path() const;

    class Writer;   // section 2.3
private:
    QFile m_file;                       // keeps the mapping alive (see 2.4)
    const uchar* m_mapped = nullptr;
    qint64 m_mappedSize = 0;
    cwPointOctreeManifest m_manifest;
    QVector<IndexEntry> m_index;        // {quint64 offset; quint64 size;} per manifest node
};
```

Return bytes by copy (`QByteArray(ptr, size)`), never `fromRawData`: a payload
outlives the source that produced it in the renderer's node table and in
in-flight loads.

### 2.2 File format

Little-endian, fixed-width integers. Version 1.

```
offset 0        header   magic "CWPK" (4 bytes) | u32 version (1)
                blobs    node payloads in build (append) order, back to back
                manifest cwPointOctreeManifest::serialize() bytes
                index    nodeCount x { u64 offset; u64 size; } -- entry i is
                         manifest node i; offset is from file start
end - 56        footer   u64 manifestOffset | u64 manifestSize |
                         u64 indexOffset   | u64 nodeCount    |
                         u64 fileSize      | u64 checksum     |
                         magic "CWPK" (4 bytes) + u32 version (1)
```

Footer is a fixed 56 bytes at the end so the reader reads it with one slice of
the mapping. `checksum` is the first 8 bytes of `QCryptographicHash::Sha1` over
the manifest bytes followed by the index bytes, and no node bytes: hashing a
several-hundred-megabyte tile at every open is too slow, and a node is already
checked by size (`loadNode` keeps its "read N bytes where the manifest says M"
error). `fileSize` must equal the real file size, so any truncation fails the
open, and so does any extra trailing data.

`open()` rejects: file shorter than header plus footer, bad magic at either
end, version mismatch, `fileSize` mismatch, checksum mismatch, an index entry
whose `offset + size` exceeds `manifestOffset`, `nodeCount` not equal to
`manifest.nodes.size()`, or a manifest that `deserialize()` rejects (that
covers `kFormatGeneration`).

### 2.3 `cwPointOctreePack::Writer`

```cpp
class CAVEWHERE_LIB_EXPORT cwPointOctreePack::Writer
{
public:
    //! Creates packPath's directory and opens packPath + ".tmp" for writing.
    //! isOpen() reports whether that worked.
    explicit Writer(const QString& packPath);
    //! Removes the temp file when finish() never succeeded.
    ~Writer();

    bool isOpen() const;

    //! Appends one node's payload under its cell key (cw::octree::cellKey of
    //! the node's level, x, y, z). Thread-safe: the builder's pass B columns
    //! call it concurrently. Returns false when the write failed. Records
    //! whatever size it is given; sizes are the reader's business.
    bool append(quint64 cellKey, const QByteArray& payload);

    //! Writes the manifest, the index in manifest node order, and the footer,
    //! flushes, closes, and renames the temp file over packPath. Fails, and
    //! leaves no pack behind, when any manifest node has no appended payload,
    //! when the same cell was appended twice, or when a write fails.
    //! errorMessage() says which.
    bool finish(const cwPointOctreeManifest& manifest);
    QString errorMessage() const;
};
```

Implementation notes:

- One `QMutex` guards `append()`: write the bytes at the current end, record
  `{offset, size}` in a `QHash<quint64, IndexEntry>` keyed by cell key, and
  advance the end. A duplicate key fails the write (return false and remember
  the message), because two payloads for one cell means a builder bug.
- `finish()` walks `manifest.nodes` in order and looks up each node's
  `cellKey(node.level, node.x, node.y, node.z)`. A node without a payload is a
  failure. Nodes appended but absent from the manifest are an error too, so
  the pack and manifest describe the same tree.
- Replace the destination with `QFile::remove(packPath)` (ignore failure when it
  is absent) then `QFile::rename(tmp, packPath)`. Use the `QFile` API, not
  `std::filesystem::rename`: on iOS the security-scoped engine is what grants
  access to a project directory from the Files picker, and both calls go
  through it. Keep the writer's `QFile` object alive until after the rename;
  the engine releases its folder scope in its destructor, not in `close()`.
- Hoist the builder's private `cellKey(const Cell&)` (cwPointOctreeBuilder.cpp
  line ~129 with `kLevelShift`, `kXShift`, `kYShift`) into `cw::octree` in
  `cwPointOctree.h` as `quint64 cellKey(int level, quint32 x, quint32 y, quint32 z)`
  plus an overload taking `const cwPointOctreeNode&`. The builder's `Cell`
  version delegates to it. The encoding must not change (level in the top bits,
  then x, y, z).

### 2.4 Mapping lifetime

`QFileDevice::map` documentation: "A file should be open for a map to succeed
but the file does not need to stay open after the memory has been mapped. When
the QFile is destroyed or a new file is opened with this object, any maps that
have not been unmapped will automatically be unmapped." So `open()` does
`QFile::open`, `map(0, size)`, validates, then `close()`, and keeps the `QFile`
member alive for the mapping. No file descriptor stays open per tile.

### 2.5 Pack path

```cpp
namespace cw::octree {
    //! <cacheRootPath>/.cw_cache/<LAZ dir mirrored the way cwDiskCacher mirrors
    //! a key path>/<laz file name>-octree<kFormatGeneration>.cwpack
    CAVEWHERE_LIB_EXPORT QString packPath(const QString& cacheRootPath, const QString& lazPath);
}
```

Implement it as `cwDiskCacher{cacheRootPath}.filePath(cwDiskCacher::Key{id, QFileInfo(lazPath).path(), {}})`
with `id = fileName + "-octree" + generation + ".cwpack"`, so the pack lands in
the same `.cw_cache/<dir>` the loose files used and the relative/absolute/
outside-root mirroring stays one implementation. The name carries the format
generation, like the old ids did; the fingerprint lives inside the manifest and
is checked on read. A rebuild of a changed tile overwrites the same pack.
`QFileInfo(lazPath)` runs once per pack path, which is once per cloud.

### 2.6 Who opens the pack

`cwPointOctreeSource` gains the pack and derives the manifest from it:

```cpp
cwPointOctreeSource(const QString& cacheRootPath, const QString& lazPath,
                    const QString& fingerprint,
                    std::shared_ptr<const cwPointOctreePack> pack);
std::shared_ptr<const cwPointOctreePack> pack;
std::shared_ptr<const cwPointOctreeManifest> manifest;  // aliases pack->manifest()
```

Build `manifest` with the aliasing constructor
`std::shared_ptr<const cwPointOctreeManifest>(pack, &pack->manifest())` so the
many readers of `source.manifest` keep working. `isNull()` also requires
`pack`. `operator==` stays identity by strings (root, lazPath, fingerprint).

`cwPointOctreeBuilder` gets `static std::shared_ptr<const cwPointOctreePack> cachedPack(const Request&)`:
fingerprint the request, `cwPointOctreePack::open(packPath(...))`, and return
it only when `pack->manifest().fingerprint == fingerprint`. `cachedManifest()`
stays for its ~20 test callers and becomes `cachedPack` then copy the manifest.
The per-node `hasEntry` loop is gone.

`cwLazLayer`:

- `startBuild()`'s probe lambda calls `cachedPack` on the worker and
  `publishOctree(std::shared_ptr<const cwPointOctreePack>)` on a hit. Update the
  comment that describes "stats one cache entry per node".
- `applyResult()` on a successful build runs the same probe
  (`cwConcurrent::run(cachedPack)`, observed with the existing generation guard)
  and publishes the opened pack. One publish path; the `QFile::open` never runs
  on the GUI thread.
- `publishOctree` takes the pack, reads the fields it copies today from
  `pack->manifest()`, and builds the source with the pack.

### 2.7 Renderer

`cwRHIPointCloud`:

```cpp
struct cwPointOctreeNodeSource {
    std::shared_ptr<const cwPointOctreePack> pack;
    int index = -1;
    qint64 byteSize = 0;
    bool operator==(const cwPointOctreeNodeSource& other) const
    { return pack == other.pack && index == other.index && byteSize == other.byteSize; }
};
```

`nodeSource(index)` returns `{m_source.pack, index, byteSize}`; `loadNode()`
does `source.pack->node(source.index)` and keeps the byte-size check and the
pick index build. Delete `m_nodeKeyBase` and the `cwDiskCacher` include. The
pack travels on every request as a `shared_ptr`, so a load in flight after
`resetNodes()` still reads a live mapping. `nodeName()` is no longer called on
the render thread for loading (it may still be used elsewhere; leave those).

### 2.8 Builder

- `ChunkContext` carries `cwPointOctreePack::Writer* writer` (the writer is
  owned by the `build()` lambda and outlives every column) instead of
  `cacheRootPath`; `lazPath` and `fingerprint` stay only if something else
  reads them.
- `build()` creates the writer right before pass B at
  `packPath(request.cacheRootPath, request.path)`; a writer that is not open
  fails the build with `CacheWriteFailed`. Every current `writeEntry(cacher, key, payload)`
  becomes `writer->append(cellKey(cell), payload)` (pass B around line 1117,
  pass C around line 1990). The manifest write around line 2146 becomes
  `writer->finish(manifest)`; its failure is `CacheWriteFailed` with
  `errorMessage()`. Every early return and cancel path leaves the writer to
  its destructor, which removes the temp file.
- `nodeNameForCell` stays only if a log or test still needs it; drop it if
  the key helpers were its only consumer.

### 2.9 Delete

After everything reads packs: `cw::octree::manifestKey`, `nodeKey` (both
overloads each), `NodeKeyBase`, `nodeKeyBase`, the private `cacheKey` in
cwPointOctree.cpp, and the `cwDiskCacher.h` include in `cwPointOctree.h`.
`cwDiskCacher` itself and its new string-arithmetic tests stay.

## 3. Tests

Tests first in each commit: write them, run them, keep the failing output,
then implement. `ASAN_OPTIONS=detect_container_overflow=0` prefixes every
`cavewhere-test` run, output is tee'd to a log, and tagged subsets are used
(the full suite takes ten minutes). Never add the user's private projects to
testcases.

### 3.1 New `testcases/test_cwPointOctreePack.cpp`, tag `[PointOctree][PointOctreePack]`

Use `QTemporaryDir`. A small synthetic manifest (3 to 10 nodes) built by hand
or via an existing helper from test_cwPointOctreeBuilder (keep helpers in the
test, not in production).

1. Round trip: append every node in a shuffled order, `finish`, `open`,
   `node(i)` equals what was appended for manifest node i, `nodeCount`,
   `manifest()` fields equal.
2. Concurrent appends from `cwConcurrent::mapped` over the nodes land once each.
3. `finish` fails and leaves no `.cwpack` and no `.tmp` when a manifest node
   was never appended; also when a cell is appended twice.
4. Destroying a writer without `finish` removes the temp file.
5. `open` returns nullptr for: missing file; file truncated by one byte; a
   byte flipped inside the manifest region (checksum); a footer `fileSize`
   that does not match (append one byte); a manifest serialized under another
   generation (patch the varint in the manifest bytes before writing, or write
   the pack with a manifest whose generation the reader rejects, whichever is
   simpler without production test hooks).
6. `node(-1)` and `node(nodeCount())` return empty.
7. `packPath` lands under `<root>/.cw_cache/<laz dir>/<name>-octree<gen>.cwpack`
   for a relative LAZ dir, an absolute LAZ dir inside the root, and one outside
   the root (mirrors `cwDiskCacher`).
8. A short payload (manifest says N bytes, appended N minus 8) still finishes
   and reads back short, so `loadNode`'s size error path remains testable.

### 3.2 Existing tests that touch the loose layout

`testcases/test_cwPointOctreeBuilder.cpp`:

- Helpers `cachedNodePoints`, `cachedQuantizedPoints`, `codedPositions`, and
  every `cacher.entry(nodeKey(...))` read: open the pack with
  `cwPointOctreePack::open(packPath(request.cacheRootPath, path))` and call
  `node(i)`.
- "moved back a generation" (around line 905): rename the pack to the previous
  generation's name (same directory, `-octree<gen-1>.cwpack`). `cachedManifest`
  must be `nullopt`.
- "a missing node makes the cached manifest a rebuild" (around line 940):
  becomes two cases, a deleted pack and a pack truncated by 1 KB.
- `restampAsGeneration` (around line 2101) and `generationKey`: become a
  rename of the pack to the older generation's pack name. Keep the three
  callers' assertions that the current-generation pack is rebuilt and reads
  back; replace `hasEntry(manifestKey(...))` checks with
  `QFileInfo::exists(packPath(...))`.
- Any assertion that the manifest key's id or path has a certain shape moves
  to `packPath`.

`testcases/test_cwPointCloudStreaming.cpp`:

- The synthetic cache helper (around line 313) writes a pack with `Writer`
  (append each node, `finish(*manifest)`) and builds the source with the opened
  pack. The `corruptNode` short payload is still appended short.
- `rootReadsBack` (around line 2027): `cachedPack` non-null and `node(root)`
  non-empty.
- The "node missing from disk" and "root node truncated" sections (around
  line 6414): delete the pack, and truncate the pack, respectively;
  `fileTimesUnder` counts stay valid with one file.
- The recode helper (around line 15323): read nodes from the source's pack,
  write a new pack with `Writer` at `packPath(newRoot, newLazPath)`, open it,
  build the source from it.
- `decodeAgainstLattice` (around line 16761): read from `source.pack`.

`testcases/test_cwPointOctree.cpp` key tests (around lines 600 to 780): replace
with `packPath` shape tests; drop the `NodeKeyBase` equality test.

`testcases/test_cwRenderPointCloud.cpp` and the other eight `cwPointOctreeSource(...)`
constructions in the streaming test: pass a pack. Where a test only needs a
non-null source with a manifest, write a minimal pack into its temp dir.

`testcases/test_cwLazLayerModel.cpp` keeps asserting `.cw_cache` exists in the
project root; it should pass unchanged.

## 4. Commits

Three commits, each building and green on its tagged subsets before the next
starts. Build directory: `build/Qt_6_11_2_for_macOS_Debug`. Build target
`cavewhere-test`.

1. **Add cwPointOctreePack** — the class, `cw::octree::cellKey` hoisted,
   `cw::octree::packPath`, and `test_cwPointOctreePack.cpp`. Nothing else
   reads or writes packs yet. Tags: `[PointOctreePack]`, `[cwDiskCacher]`.
2. **Write and read point octree packs** — builder, `cachedPack`, source,
   layer, renderer, and every test in section 3.2. Tags: `[PointOctree]`,
   `[PointOctreeBuilder]`, `[PointCloudStreaming]`, `[LazLayer]`,
   `[cwLazLayerModel]`, `[RenderPointCloud]` (check the exact tags with
   `cavewhere-test --list-tags`).
3. **Remove the per-node cache keys** — section 2.9 deletions and the
   `test_cwPointOctree.cpp` key tests. Tags as in commit 2.

Commit rules: imperative capitalized subject, body with what and why and the
test commands run, no Co-Authored-By line, never amend, never push.

## 5. Open items for the user

- iOS verify: with a project opened from the Files picker, the pack's
  `QFile::open` goes through the handler once per tile and the mapping reads
  afterward. Profile the worker thread again; `QFile::open` should be gone
  from the per-node path.
- Windows: `finish()` renames the old pack to a unique `.staleN` name and
  sweeps stale copies on each finish, so a rebuild of a displayed tile
  succeeds on Windows and a still-mapped stale copy is removed by a later
  rebuild.
- Old-generation and old-layout files are left behind in `.cw_cache`, as
  before. A sweep is a separate task.
