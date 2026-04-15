# Fix EXIF Rotation in V6 Project Conversion

## Context

When converting v6 `.cw` (SQLite) projects to v9, images with EXIF rotation have broken scrap coordinates. The current code in `saveAllFromV6()` is experimental with commented-out alternatives, debug logging, and an unused `r` variable. It needs to be cleaned up, refactored, and properly tested.

### Two coordinate systems need fixing

Both have pre-rotation `originalSize` (e.g. 4032x3024 for a portrait photo), but the coordinates are in different spaces:

**1. Landscape coords (v1-v5, and v6 files carrying v5 data)**
- No EXIF auto-rotation during display
- User drew scraps on the **landscape** image (raw 4032x3024)
- `toNormalized()` used landscape `originalSize` — correct for that display
- Coords: x in [0,1], y in [0,1]
- **Fix**: `transformForOrientation(exifTransform)` — geometric rotation in [0,1] normalized space
- Example Rotate90: (x,y) -> (y, 1-x). Verified: (0.660, 0.620) -> (0.620, 0.340) matches 2025.2-101

**2. Distorted portrait coords (v6)**
- Qt6 `QImage::fromData()` auto-rotated on some platforms -> portrait display (3024x4032)
- But `toNormalized()` still used landscape `originalSize` (4032x3024) to normalize
- Portrait pixels divided by landscape dims = wrong normalization
- Coords: x in [0, H/W ~0.75], y in [1-W/H ~-0.33, 1] — y can go **NEGATIVE**
- **Fix**: re-normalize from landscape dims to portrait dims
- Math: `correct_x = stored_x * preW/postW`, `correct_y = stored_y * preH/postH + (1 - preH/postH)`
- This is an affine transform (QTransform): `scale(sx, sy)` + `translate(0, 1-sy)` where sx=preW/postW, sy=preH/postH

**3. Correct portrait coords (v6+, CaveWhere 2025.2-101+)**
- `originalSize` stored as post-rotation (3024x4032)
- No fix needed

### Why the `r` code exists (and why it's messy)

The commented-out `mapPoint` with `r = W/H` IS the re-normalization for case 2 — it's mathematically correct but unexplained. The clean derivation:
```
stored_x = portrait_px / landscape_W     (wrong normalization)
correct_x = portrait_px / portrait_W     (correct normalization)
correct_x = stored_x * landscape_W / portrait_W = stored_x * W/H = stored_x * r
```

### Detection algorithm — file-level scan

**Verified against all test files using extracted coordinates:**

| File | Proto | originalSize | y<0? (file scan) | x>H/W? | Detection | Fix |
|------|-------|-------------|------------------|--------|-----------|-----|
| backgroundRotation-0.08.cw | v1 | 4032x3024 | no | no | proto<6 | Rotation |
| backgroundRotation-0.09.cw | v3 | 4032x3024 | no | no | proto<6 | Rotation |
| backgroundRotation-2025.2.cw | v5 | 4032x3024 | no | no | proto<6 | Rotation |
| backgroundRotation-2025.2 to 2025.2-101.cw | v6 | 4032x3024 | no | no | **fallback** | **?** |
| backgroundRotation-2025.2-101.cw | v6 | 3024x4032 | — | — | post-rot | No fix |
| TB and CP map (2).cw | v6 | mixed | YES (Trip1) | no | y<0 scan | Re-normalization |

**Algorithm:**
1. If stored `originalSize` matches `imageInfo()` (post-rotation) -> **no fix**
2. If proto version < 6 -> **landscape** (rotation)
3. If proto version >= 6, check **per-note** scrap coords for distorted portrait signal:
   - Any y < 0 in the note's scraps -> **distorted portrait** (re-normalization)
   - Otherwise -> **landscape** (rotation) — v5 coords carried into v6

**Fallback rationale**: `backgroundRotation-2025.2 to 2025.2-101.cw` is v6 with v5 coords and no y<0. Defaulting to landscape (rotation) handles it correctly. For TB and CP map, notes with distorted coords have y<0 in their own scraps.

**Risk**: A v6 note with distorted portrait coords where ALL scraps are small and centered (no y<0) would be misdetected. This seems unlikely in practice — cave survey scraps typically cover large areas of the note image.

## V9 runtime audit — no changes needed

The current v9 coordinate pipeline is correct. The full chain uses post-rotation dimensions consistently:

| Step | Size used | Source |
|------|-----------|--------|
| Image display (`QQ.Image` in `ImageBackground.qml`) | Post-rotation | `cwImageProvider::image()` → `imageWithAutoTransform()` (line 503) |
| `originalSize` on import | Post-rotation | `imageInfo()` → `imageWithAutoTransform()` (line 215) |
| `renderSize()` (`cwNote.cpp:232`) | Post-rotation | Returns `originalSize()` for pixel images |
| `toNormalized()` / `toImage()` (`cwScrapView.cpp:153`) | Post-rotation | Uses `renderSize()` |
| Click → stored coord | [0,1] correct | Click pixel position / post-rotation dims |
| V9 file load (`loadImage()`, `cwSaveLoad.cpp:250`) | Post-rotation | Trusts stored size; re-reads with `imageWithAutoTransform()` if missing |

**Key code path**: User clicks on note image → `cwBaseScrapInteraction::snapToScrapLine()` → `cwScrapView::toNormalized(note())` → divides by `renderSize()` (post-rotation) → normalized [0,1] coords stored in scrap.

The `QQ.Image` in `ImageBackground.qml` uses `fillMode: Image.Pad` (no rescaling), so its width/height equals the auto-transformed source size. This matches `renderSize()` which matches `originalSize()` — all post-rotation.

**Conclusion**: The re-normalization fix only applies during v6→v9 conversion in `saveAllFromV6()`. No changes needed to the v9 runtime coordinate pipeline.

## Implementation

### 1. Extract utilities to `cwImageUtils`

**File**: `cavewherelib/src/cwImageUtils.h` / `.cpp`

```cpp
/// Maps normalized [0,1] coords from pre-EXIF space to post-EXIF space
static QTransform transformForOrientation(QImageIOHandler::Transformations t);

/// Re-normalizes coords from wrongSize normalization to correctSize normalization,
/// accounting for CaveWhere's Y-flip convention in toNormalized().
/// Used when the display was auto-rotated but coords were normalized to pre-rotation dims.
static QTransform reNormalizationTransform(const QSize& wrongSize, const QSize& correctSize);
```

`reNormalizationTransform` implementation:
```cpp
QTransform cwImageUtils::reNormalizationTransform(const QSize& wrongSize, const QSize& correctSize) {
    const double sx = double(wrongSize.width()) / correctSize.width();
    const double sy = double(wrongSize.height()) / correctSize.height();
    // Maps: x' = x*sx, y' = y*sy + (1-sy)
    QTransform t;
    t.translate(0.0, 1.0 - sy);
    t.scale(sx, sy);
    return t;
}
```

Move `transformForOrientation` from the lambda in `cwSaveLoad.cpp:4019-4068`. Remove all `qDebug()`.

### 2. Add `fileVersion()` getter to `cwProject.h`

`int fileVersion() const { return FileVersion; }` — `FileVersion` (line 259) is private with no getter. This is the proto version from `CavingRegion.version` in the v6 SQLite blob (1, 3, 5, or 6). **Not** `d->lastLoadMaxFileVersion` which is 0 for all v6 files (it tracks per-entity v9 headers).

### 3. Clean up `cwSaveLoad.cpp::saveAllFromV6()` EXIF section

**Note**: `d->lastLoadMaxFileVersion` (line 3939, captured as `version`) is **always 0** for v6 files — it tracks v9 per-entity headers, not the v6 proto version. Use `project->fileVersion()` instead.

Add at top of `saveAllFromV6()` (after line 3939):

```cpp
const int protoVersion = project->fileVersion();
```

Capture `protoVersion` in the `saveNotes` lambda (line 3941).

**Per-note EXIF fix** — replace the current experimental block (~lines 3978-4131):

```cpp
{
    QByteArray rawBytes = imageData.data();
    QBuffer buf(&rawBytes);
    buf.open(QIODevice::ReadOnly);
    QImageReader reader(&buf);
    const auto transform = reader.transformation();

    if (transform != QImageIOHandler::TransformationNone) {
        const auto info = imageInfo(filename);
        const QSize storedSize = noteCopy.image().originalSize();
        const bool needsCoordFix = (storedSize != info.originalSize);

        if (needsCoordFix) {
            QTransform coordFix;
            if (protoVersion < 6) {
                // V1-V5: coords in landscape space, apply EXIF rotation
                coordFix = cwImageUtils::transformForOrientation(transform);
            } else {
                // V6: check this note's scrap coords for distorted portrait signal
                bool hasDistortedCoords = false;
                for (const cwScrap* scrap : noteCopy.scraps()) {
                    for (const QPointF& pt : scrap->points()) {
                        if (pt.y() < 0.0) { hasDistortedCoords = true; break; }
                    }
                    if (!hasDistortedCoords) {
                        for (const cwNoteStation& st : scrap->stations()) {
                            if (st.positionOnNote().y() < 0.0) { hasDistortedCoords = true; break; }
                        }
                    }
                    if (!hasDistortedCoords) {
                        for (const cwLead& lead : scrap->leads()) {
                            if (lead.positionOnNote().y() < 0.0) { hasDistortedCoords = true; break; }
                        }
                    }
                    if (hasDistortedCoords) break;
                }

                if (hasDistortedCoords) {
                    // Auto-rotated display but toNormalized used wrong dims
                    coordFix = cwImageUtils::reNormalizationTransform(storedSize, info.originalSize);
                } else {
                    // v5 landscape coords carried into v6 file
                    coordFix = cwImageUtils::transformForOrientation(transform);
                }
            }

            for (cwScrap* scrap : noteCopy.scraps()) {
                QVector<QPointF> outline = scrap->points();
                for (auto& pt : outline) { pt = coordFix.map(pt); }
                scrap->setPoints(outline);

                auto stations = scrap->stations();
                for (auto& station : stations) {
                    station.setPositionOnNote(coordFix.map(station.positionOnNote()));
                }
                scrap->setStations(stations);

                auto leads = scrap->leads();
                for (auto& lead : leads) {
                    lead.setPositionOnNote(coordFix.map(lead.positionOnNote()));
                }
                scrap->setLeads(leads);
            }
        }

        // Always set correct post-rotation originalSize + DPI
        cwImage img = noteCopy.image();
        img.setOriginalImageInfo(info);
        noteCopy.setImage(img);
    }
}
```

**Remove**: all `qDebug()`, commented-out code, unused `r` variable.

### 4. Remove debug code

- **`cwTriangulateTask.cpp`**: Remove debug block lines 170-307 (`BEGIN TEMPORARY DEBUG` to `END TEMPORARY DEBUG`)
- **`test_cwTriangulateTask.cpp`**: Remove `[.cwTriangulateDebug]` test case
- **`cwSaveLoad.cpp`**: Remove all debug `qDebug()` statements in the EXIF section

### 5. EXIF in `cwTriangulateTask.cpp` — no code changes needed

Triangulation handles EXIF transparently: `cwImageProvider::requestImage()` calls `cwImageUtils::imageWithAutoTransform()` with `setAutoTransform(true)`. The image arrives already rotated. `scrap.noteImage().originalSize()` at line 138 must be post-rotation for correct cropping — ensured by the v6 conversion setting `imageInfo()`.

### 6. Update `docs/cavewhere-format-spec.md`

Add a **Scrap Coordinate System** section after the Scrap protobuf docs:
- Coordinates stored in normalized [0,1] space relative to `Image.size` (post-EXIF-rotation dimensions)
- Y-axis convention: 0 = bottom of image, 1 = top (OpenGL convention, opposite of pixel space)
- `toImage()` transform: `scale(width, height) * scale(1, -1) * translate(0, -1)`
- `Image.size` always stores post-EXIF-rotation dimensions in v9+
- V6 legacy conversion applies coordinate transforms to match this convention

## Files to modify

| File | Changes |
|------|---------|
| `cavewherelib/src/cwImageUtils.h` | Add `transformForOrientation()` and `reNormalizationTransform()` |
| `cavewherelib/src/cwImageUtils.cpp` | Implement both utilities |
| `cavewherelib/src/cwProject.h` | Add `fileVersion()` getter |
| `cavewherelib/src/cwSaveLoad.cpp` | Clean EXIF fix with detection, remove debug |
| `cavewherelib/src/cwTriangulateTask.cpp` | Remove debug block (lines 170-307) |
| `testcases/test_cwTriangulateTask.cpp` | Remove `[.cwTriangulateDebug]` test |
| `testcases/test_cwScrapManager.cpp` | Update/add EXIF conversion tests |
| `testcases/test_cwScrapManager.qrc` | Add test fixture references |
| `docs/cavewhere-format-spec.md` | Add scrap coordinate system section |

## Test plan

### Test fixtures

Copy the 5 backgroundRotation `.cw` files into `testcases/datasets/test_cwScrapManager/`. For TB and CP map, use skip-if-missing (it's a user file, not a test fixture).

### Test cases in `test_cwScrapManager.cpp`

1. **"V6 conversion with landscape EXIF coords applies rotation"** (v1 file)
   - Load `backgroundRotation-0.08.cw`, verify `originalSize` becomes 3024x4032
   - Verify station coords are rotated: (0.660, 0.620) -> approx (0.620, 0.340)
   - Verify DPI is set from image

2. **"V6 conversion with v5 coords in v6 file applies rotation"**
   - Load `backgroundRotation-2025.2 to 2025.2-101.cw`
   - Verify same rotation behavior as v1-v5 files

3. **"V6 conversion with correct EXIF coords does not modify"**
   - Load `backgroundRotation-2025.2-101.cw` (post-rotation originalSize)
   - Verify coords unchanged, originalSize unchanged

4. **"V6 conversion with distorted portrait coords applies re-normalization"** (if TB file available)
   - Load TB and CP map, verify y<0 detection triggers re-normalization
   - Verify coords are re-normalized, not rotated

5. **"V6 conversion triangulation succeeds"**
   - All converted files triangulate successfully

### Unit tests for utilities

6. **"transformForOrientation Rotate90 maps (x,y) to (y, 1-x)"**
7. **"reNormalizationTransform corrects distorted portrait coords"**

### Verification

1. Run `[cwScrapManager]` and `[cwTriangulateTask]` tests
2. Run full `cavewhere-test` and `cavewhere-qml-test` suites
3. Manual: load each .cw file and verify carpet/scrap texture renders correctly
