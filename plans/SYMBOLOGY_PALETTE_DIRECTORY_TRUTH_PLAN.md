# Palette directory-as-truth refactor (commit 3.1)

Slot **after commit 3**, before commit 4, of `SYMBOLOGY_PALETTE_PLAN.html`. A
single behavior-preserving commit, **3.1**. Pure persistence-format change: the
in-memory model, render path, snapshot, seed, and manager are untouched — only
`cavewhere_symbology.proto` and `cwSymbologyPaletteIO` change.

## Motivation

Commit 3 stored a palette directory like this:

```
<palette>/
  palette.json     # id, name, author, version + ALL lineBrushes inline
                   #                            + a glyph METADATA index (name + displayName)
  glyphs/
    floor-step-tick.cwglyph   # name, displayName, strokes
```

Glyphs got their own files to keep git diffs clean and avoid merge conflicts —
but the design only went halfway:

- **Two sources of truth.** `displayName` lives in *both* `palette.json`
  (`GlyphMetadata`) and the `.cwglyph` file; `name` is in the index, the file
  payload, *and* the filename. That is the smell behind the load-time
  name-mismatch check.
- **The index reintroduces the conflict it was meant to remove.** Adding,
  removing, or renaming a glyph rewrites `palette.json`. Two cartographers each
  adding a symbol on separate branches conflict on `palette.json` even though
  their glyph *files* merge cleanly.
- **Brushes are worse.** They're stored inline in `palette.json`, so *any*
  brush edit conflicts with *any* other brush edit.

A palette is already a directory (the manager scans subdirectories of
`AppDataLocation` and dedups by UUID), so the fix is to make the directory the
source of truth.

## Decisions (settled)

1. **Ordering: derive it, no stored order.** Load = scan + sort by name.
   Brushes already carry `zOrder` (paint order) and `category` (picker
   grouping) inside each file; pickers sort by category then `displayName`.
   Glyphs sort by `displayName`. No central order list, no per-entity sort key.
2. **Deletion: `save()` reconciles.** A full palette `save()` deletes any
   `brushes/*.cwbrush` / `glyphs/*.cwglyph` whose `<name>` is not in the current
   in-memory set, so disk always matches memory after a save. Deletion is
   scoped strictly to those two suffixes in those two subdirs — never touches
   anything else.

## Target on-disk layout

```
<palette>/
  palette.json     # IDENTITY ONLY: id, name, description, author, version
  brushes/
    wall.cwbrush
    scrap-outline.cwbrush
    feature.cwbrush
    floor-step.cwbrush
  glyphs/
    floor-step-tick.cwglyph
```

- **Membership = files present.** No index. The set of `*.cwbrush` /
  `*.cwglyph` files *is* the palette — git's model, so independent adds merge
  with zero conflict. (This also dissolves the "orphan glyph files" review
  finding: every file counts, there are no orphans.)
- **One file per editable thing.** Editing a brush rewrites one file; adding a
  glyph is one new file and nothing else.
- `palette.json` stays but holds only palette-level identity, which changes
  rarely — so it almost never conflicts.
- **Filename is authoritative for the name.** The payload still carries `name`
  (self-describing files); load validates `filenameStem == payload.name`, same
  guard glyphs already have. `displayName` lives only in the file now — the
  duplication is gone with the index.

## Proto changes (`cavewhere_symbology.proto`)

- `Palette` becomes identity-only: keep `id`, `name`, `description`, `author`,
  `version`. **Remove** `repeated LineBrush lineBrushes = 6;` and
  `repeated GlyphMetadata glyphs = 7;`; mark fields 6 and 7 `reserved`.
- **Delete** the `GlyphMetadata` message.
- `LineBrush` becomes the per-file payload for `brushes/<name>.cwbrush`, exactly
  as `Glyph` is already the payload for `glyphs/<name>.cwglyph`. No new message
  — `LineBrush` and `Glyph` are unchanged; only where they're written changes.
- Update the `Palette` / `Glyph` doc comments (drop the "glyph index" / "must
  match the palette's GlyphMetadata entry" wording).

## IO changes (`cwSymbologyPaletteIO.{h,cpp}`)

New constants alongside the glyph ones:

```cpp
extern const QString kBrushesSubdirName; // "brushes"
extern const QString kBrushFileSuffix;   // ".cwbrush"
```

**`palette.json` is identity-only.** `paletteToProto` / `paletteFromProto`
write/read only the identity fields. `toJson` / `fromJson` therefore round-trip
identity only; `fromJson` returns a `cwSymbologyPaletteData` with empty
`lineBrushes` / `glyphs` (the directory layer fills those). Keep the null-id
rejection.

**Per-entity file IO (mirror the existing glyph functions):**

```cpp
Monad::ResultBase           saveBrush(const cwLineBrush &brush, const QString &directory);
Monad::Result<cwLineBrush>  loadBrush(const QString &directory, const QString &brushName);
QByteArray                  brushToJson(const cwLineBrush &brush);
Monad::Result<cwLineBrush>  brushFromJson(const QByteArray &json);
```

`saveBrush` validates the name (see below), `mkpath`s `brushes/`, writes
`brushes/<name>.cwbrush` via `QSaveFile`. `loadBrush` reads one file. These
reuse the existing `brushToProto` / `brushFromProto` and the generic
`serializeJson(const google::protobuf::Message&)`.

**`save(palette, dir)`** becomes:

1. validate `dir`, `mkpath`.
2. write `palette.json` (identity only).
3. for each brush → `saveBrush`; for each glyph → `saveGlyph`.
4. **reconcile:** list `brushes/*.cwbrush`, delete any whose stem is not in
   `palette.lineBrushes`; same for `glyphs/*.cwglyph` vs `palette.glyphs`.
   Scoped to those suffixes/subdirs only.

**`load(dir)`** becomes:

1. read `palette.json` → identity (error if the file is missing).
2. scan `brushes/*.cwbrush`, sorted by name (`QDir::entryList(..., QDir::Name)`)
   → `loadBrush` each → `lineBrushes`. Validate name and `stem == payload.name`.
3. scan `glyphs/*.cwglyph`, sorted → `loadGlyph` each → `glyphs`. (Same checks;
   `loadGlyph` / `glyphFromProto` already `recomputeBounds()`.)
4. run `findGlyphDependencyCycle()` and reject a cycle (unchanged).

**Name validation** is already lifted into the shared free function
`cwSymbology::isValidName(QStringView)` (`cwSymbologyName.{h,cpp}`) and applied to
both brush *and* glyph names at load. This commit only needs to also call it
from the new `saveBrush` (mirroring the existing `saveGlyph` guard) — both are
now path components.

## What does **not** change

- `cwSymbologyPaletteData` in memory (`QVector<cwLineBrush>` +
  `QVector<cwSymbologyGlyph>`), `snapshot()`, `brush()` / `glyph()`,
  `findGlyphDependencyCycle()`, `findDuplicate*Name()`.
- `cwSymbologyPalette`, `cwSymbologyPaletteSeed::create()` (builds the same data;
  only its on-disk projection differs).
- `cwSymbologyPaletteManager::reload()` — still calls `load(dir)` per
  subdirectory; the format change is invisible to it.
- The render path / tessellation cache — none of it reads `palette.json`.

## Tests

Update (format moved out of `palette.json`):

- "Palette JSON round-trips brushes and glyph metadata" → **identity round-trip
  only** (id/name/description/author/version; `lineBrushes` and `glyphs` empty
  after a `toJson`/`fromJson` cycle).
- "Color hex round-trips opaque and translucent values" → exercise it through
  `saveBrush`/`loadBrush` (or `brushToJson`/`brushFromJson`), since brushes no
  longer travel in `palette.json`.
- "Loading a palette with duplicate brush names fails" (the `fromJson` form) →
  drop or repurpose: filenames are unique by construction, so duplicate brush
  *files* can't exist. Keep `findDuplicateBrushName` as a guard for
  programmatic construction; cover it at the data layer, not IO.
- "Palette saves and loads from a directory" (full seed) → keep as the anchor;
  additionally assert `brushes/wall.cwbrush` and `glyphs/floor-step-tick.cwglyph`
  exist and `loaded == source`.

Add:

- **Scan discovers entities (no manifest).** Save the seed, drop a new valid
  `brushes/extra.cwbrush` into the dir *without* editing `palette.json`, reload,
  and confirm the new brush appears — the headline merge-friendly behavior.
- **`save()` reconciles deletes.** Save the seed, then `save()` a palette with
  one brush and one glyph removed; assert their files are gone and the others
  (byte-for-byte) remain.
- **Edit isolation for brushes.** Mirror "Editing one glyph leaves the other
  glyph files untouched" for `saveBrush`.
- **Deterministic order.** A two-brush palette written in one order loads sorted
  by name regardless of write order.
- **Path-traversal rejection for brush names** (mirror the glyph test);
  `saveBrush("../pwned")` errors and writes nothing.
- **Filename/payload name mismatch fails** for brushes (mirror the glyph test).

Per the repo convention, the brush-file round-trip / reconcile tests live in
`test_cwSymbologyPaletteIO.cpp` alongside the existing glyph-file tests.

## Out of scope / deferred

- **`.cwpalette` single-file bundle.** Directory-as-truth zips cleanly into the
  planned bundle for sharing/distribution; that stays in its own later commit.
- **Dangling cross-reference validation** (a glyph stroke naming a missing
  brush, a decoration layer naming a missing glyph). Not required here; the
  cycle check stays. Add later if the editor needs it.
- **`AreaBrush` / `PointBrush`** (proto fields 8/9, still reserved).

## Dependency

Follows commit 3 (which introduced the `GlyphMetadata` index and the inline
brushes this commit removes). Lands before commit 4 (placement rules /
decoration layout) so the on-disk format is settled before anything depends on
reading palettes at scale.

## Checklist

- [x] Proto: `Palette` identity-only (6/7 reserved); delete `GlyphMetadata`.
- [x] IO: `kBrushesSubdirName` / `kBrushFileSuffix`; `saveBrush` / `loadBrush` /
      `brushToJson` / `brushFromJson`.
- [x] IO: `paletteToProto`/`paletteFromProto`/`toJson`/`fromJson` identity-only.
- [x] IO: `save()` writes per-file + reconciles deletes; `load()` scans + sorts.
- [x] Name validation (`cwSymbology::isValidName`) and filename↔payload-name
      checks centralized in shared `writeNamedFile` / `scanEntities` helpers,
      covering brushes and glyphs uniformly.
- [x] Tests updated + added (above).
- [x] `cavewhere-test` green for
      `[cwSymbologyPalette][cwSymbologyPaletteIO][cwSymbologyPaletteManager]`.

> Implementation note: the per-entity name validation and filename/payload
> name-mismatch check are now factored into two file-local helpers in
> `cwSymbologyPaletteIO.cpp` — `writeNamedFile` (validate → mkpath → atomic
> write) and the `scanEntities` template (sorted scan → validate stem → load →
> check `stem == payload.name`) — shared by the brush and glyph paths, plus a
> `reconcileDir` helper for the delete reconciliation. The seed
> (`cwSymbologyPaletteSeed::create`) now appends brushes in name-sorted order so
> it compares equal to its own reload (order is derived, not stored).
