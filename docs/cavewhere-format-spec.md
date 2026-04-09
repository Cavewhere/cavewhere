# CaveWhere File Format Specification

**Format Version:** 9 (`2026.4`)
**Last Updated:** 2026-04-09

## Overview

CaveWhere stores cave survey projects as a **Git-backed directory tree** of JSON-serialized
Protocol Buffer files plus binary assets (images, 3D models). Two packaging modes exist:

| Mode | Extension | Description |
|------|-----------|-------------|
| **Directory** | `.cwproj` | Git repository on disk; primary working format |
| **Bundle** | `.cw` | ZIP archive of a directory project (with `.git/`); for sharing/backup |

Both modes contain the same logical structure. A bundle is a zipped directory project
**including the `.git/` repository**, which preserves full history for sync and merge
operations. Opening a bundle extracts it to a temporary directory; saving re-zips.

A legacy **SQLite** format (also `.cw`, version 6 and earlier) is auto-converted to directory
format on load. It is read-only and not documented further here.

---

## Directory Layout

```
<project-root>/
+-- <RegionName>.cwproj                       # Project descriptor
+-- .git/                                      # Git repository
|   +-- info/exclude                           # Local exclusions (.cw_cache/, etc.)
+-- .gitattributes                             # LFS tracking rules
+-- <dataRoot>/                                # Data directory (see Project.metadata.dataRoot)
|   +-- <CaveName>/
|   |   +-- <CaveName>.cwcave                  # Cave metadata
|   |   +-- trips/
|   |       +-- <TripName>/
|   |           +-- <TripName>.cwtrip          # Trip survey data
|   |           +-- notes/
|   |               +-- <NoteName>.cwnote      # 2D survey note
|   |               +-- <NoteName>.cwnote3d    # LiDAR 3D note
|   |               +-- survey-scan.png        # Image asset (referenced by .cwnote)
|   |               +-- model.glb              # 3D asset (referenced by .cwnote3d)
+-- .cw_cache/                                 # Local cache (git-excluded, not in bundles)
```

### Path Rules

- **dataRoot** is a relative path stored in `Project.metadata.dataRoot`. It defaults to the
  sanitized region name. All cave directories live under it.
- Directory and file names are **sanitized** versions of user-visible entity names
  (see [Name Sanitization](#name-sanitization)).
- All stored paths use forward slashes (`/`). Relative paths are resolved from `dataRoot`.

### File Extensions

| Extension | Entity | Serialization |
|-----------|--------|---------------|
| `.cwproj` | Project | JSON protobuf (`CavewhereProto.Project`) |
| `.cwcave` | Cave | JSON protobuf (`CavewhereProto.Cave`) |
| `.cwtrip` | Trip | JSON protobuf (`CavewhereProto.Trip`) |
| `.cwnote` | 2D Note | JSON protobuf (`CavewhereProto.Note`) |
| `.cwnote3d` | LiDAR Note | JSON protobuf (`CavewhereProto.NoteLiDAR`) |

---

## Protobuf Schema

All entity files are **JSON-encoded Protocol Buffer messages** (not binary protobuf).
The schema is defined in `cavewherelib/src/cavewhere.proto` and `cavewherelib/src/qt.proto`.
JSON is produced with `google::protobuf::util::MessageToJsonString()` with whitespace enabled
for human readability.

**JSON conventions:**
- Field names use **camelCase** (protobuf's default JSON mapping), e.g., `fileVersion`,
  `backCompass`, `includeDistance`.
- Enum values are serialized as **string names**, not integers (e.g., `"Feet"`, `"Plan"`).
- Fields with **default/zero values are omitted** from the JSON output. Readers must treat
  missing fields as their protobuf defaults (0 for numbers, false for bools, empty for strings).
- **Exception — `includeDistance`**: The protobuf default is `false`, but the application
  default is `true`. When this field is absent on a shot row, readers **must treat it as
  `true`** (shot is active). It is only explicitly set to `false` when a shot is excluded.
- Unknown fields are **silently ignored** on deserialization (forward compatibility).

### Project (`.cwproj`)

```protobuf
message Project {
    optional FileVersion fileVersion = 1;
    optional string name = 2;              // Display name
    optional ProjectMetadata metadata = 3;
    optional string id = 4;               // UUID, stable across renames
}

message ProjectMetadata {
    optional string dataRoot = 1;          // Relative path to data directory
    optional bool syncEnabled = 3;         // Whether remote sync is enabled
}
```

### Cave (`.cwcave`)

```protobuf
message Cave {
    optional FileVersion fileVersion = 9;
    optional string name = 8;
    optional string id = 10;               // UUID
    optional Units.LengthUnit lengthUnit = 3;
    optional Units.LengthUnit depthUnit = 4;
}
```

Caves are independent files. Trips are **not** embedded in the cave file; they are
discovered by scanning the `trips/` subdirectory.

### Trip (`.cwtrip`)

```protobuf
message Trip {
    optional FileVersion fileVersion = 8;
    optional string name = 7;
    optional string id = 9;               // UUID
    optional QtProto.QDate date = 2;
    optional TripCalibration tripCalibration = 4;
    repeated SurveyChunk chunks = 5;      // Survey data
    optional Team team = 6;
}
```

Notes are **not** embedded in the trip file; they are discovered by scanning the `notes/`
subdirectory.

#### Survey Data

Survey data is stored as an array of `SurveyChunk` messages. Each chunk contains an
interleaved array of `StationShot` messages representing the survey leg table:

```protobuf
message SurveyChunk {
    repeated StationShot leg = 4;         // Interleaved station/shot data
    optional string id = 5;              // UUID
}
```

The `leg` array alternates between station rows and shot rows:
`Station -> Shot -> Station -> Shot -> Station`

```protobuf
message StationShot {
    optional string id = 1;              // UUID

    // Station fields (populated on station rows)
    optional string name = 100;          // Station name (e.g., "A1")
    optional string left = 101;          // LRUD measurements as strings
    optional string right = 102;
    optional string up = 103;
    optional string down = 104;

    // Shot fields (populated on shot rows)
    optional bool includeDistance = 1000; // Whether this shot is active (app default: true)
    optional string distance = 1001;     // Tape reading
    optional string compass = 1002;      // Front compass bearing
    optional string backCompass = 1003;  // Back compass bearing
    optional string clino = 1004;        // Front inclination
    optional string backClino = 1005;    // Back inclination
}
```

All measurement values are stored as **strings**, not numbers. This preserves
the user's exact input (e.g., empty strings for missing readings vs. `"0"` for explicit zero).

#### Calibration

```protobuf
message TripCalibration {
    optional double tapeCalibration = 3;
    optional double frontCompassCalibration = 4;
    optional double frontClinoCalibration = 5;
    optional double backCompassCalibration = 14;
    optional double backClinoCalibration = 7;
    optional double declination = 8;
    optional Units.LengthUnit distanceUnit = 9;
    optional bool correctedCompassBacksight = 1;
    optional bool correctedClinoBacksight = 2;
    optional bool correctedCompassFrontsight = 12;
    optional bool correctedClinoFrontsight = 13;
    optional bool frontSights = 10;
    optional bool backSights = 11;
}
```

#### Team

```protobuf
message Team {
    repeated TeamMember teamMembers = 1;
}

message TeamMember {
    optional string id = 4;              // UUID
    optional string name = 3;
    repeated string jobs = 5;            // e.g., ["surveyor", "scribe"]
}
```

### 2D Note (`.cwnote`)

```protobuf
message Note {
    optional FileVersion fileVersion = 6;
    optional string id = 7;              // UUID
    optional string name = 5;
    optional Image image = 1;
    optional double rotation = 2;        // Degrees
    optional ImageResolution imageResolution = 4;
    repeated Scrap scraps = 3;
}
```

#### Image Reference

Notes reference image files by filename only (no directory path). The image file lives in
the same `notes/` directory as the `.cwnote` file.

```protobuf
message Image {
    enum Unit {
        Pixels = 0;    // Raster images (PNG, JPEG)
        Points = 1;    // PDF (72 points per inch)
        SvgUnits = 2;  // SVG coordinate units
    }

    optional string path = 7;           // Filename only (e.g., "scan.png")
    optional QtProto.QSize size = 4;    // Original dimensions in image units
    optional int32 dotPerMeter = 5;     // Resolution
    optional int32 page = 8;            // PDF page number (0-based)
    optional Unit imageUnit = 10;       // Unit type for size/DPI interpretation
}
```

- Multi-page PDFs produce one `.cwnote` per page, each with the same `image.path` but
  different `image.page` values.
- Image files are **copied** into the project on import. The `path` field stores only the
  filename, never an absolute or relative directory path.

#### Scraps

Scraps are digitized outlines drawn on top of survey note images. They contain the geometric
data needed for 2D-to-3D morphing.

```protobuf
message Scrap {
    enum ScrapType {
        Plan = 0;
        RunningProfile = 1;
        ProjectedProfile = 2;
    }

    optional string id = 9;             // UUID
    optional ScrapType type = 7;
    repeated QtProto.QPointF outlinePoints = 1;   // Polygon vertices (image coords)
    repeated NoteStation noteStations = 2;         // Station markers
    optional NoteTransformation noteTransformation = 3;  // Georeferencing
    optional bool calculateNoteTransform = 4;
    repeated Lead leads = 6;
    optional ProjectedProfileScrapViewMatrix profileViewMatrix = 8;
}

message NoteStation {
    optional string id = 4;             // UUID
    optional string name = 3;           // Station name (must match a station in survey data)
    optional QtProto.QPointF positionOnNote = 2;  // Position in image coordinates
}

message NoteTransformation {
    optional double northUp = 1;        // Compass north direction (degrees)
    optional Length scaleNumerator = 2;  // e.g., 1 inch on paper
    optional Length scaleDenominator = 3; // e.g., 100 feet in cave
}

message Length {
    optional double value = 1;
    optional Units.LengthUnit unit = 2;
}

message ImageResolution {
    optional double value = 1;
    optional Units.ImageResolutionUnit unit = 2;
}

message Lead {
    optional string id = 6;             // UUID
    optional QtProto.QPointF positionOnNote = 1;
    optional QtProto.QSizeF size = 3;
    optional string description = 5;
    optional bool completed = 4;
}

message ProjectedProfileScrapViewMatrix {
    enum Direction {
        LookingAt = 0;
        LeftToRight = 1;
        RightToLeft = 2;
    }
    optional double azimuth = 1;
    optional Direction direction = 2;
}
```

### LiDAR Note (`.cwnote3d`)

```protobuf
message NoteLiDAR {
    optional FileVersion fileVersion = 6;
    optional string id = 7;             // UUID
    optional string name = 1;
    optional string fileName = 2;       // 3D asset filename (e.g., "model.glb")
    repeated NoteLiDARStation noteStations = 3;
    optional NoteLiDARTransformation noteTransformation = 4;
    optional bool autoCalculateNorth = 5;
}

message NoteLiDARStation {
    optional string name = 1;           // Station name
    optional QtProto.QVector3D positionOnNote = 2;  // 3D coordinates
    optional string id = 3;            // UUID
}

message NoteLiDARTransformation {
    enum UpMode {
        Custom = 0;
        XisUp = 1;
        YisUp = 2;
        ZisUp = 3;
    }

    optional NoteTransformation planTransform = 1;
    optional UpMode upMode = 2;
    optional float upSign = 3;          // +1 or -1
    optional QtProto.QQuaternion upCustom = 4;  // Custom orientation quaternion
}
```

### Qt Helper Types (`qt.proto`)

```protobuf
message QDate   { int32 year = 1; int32 month = 2; int32 day = 3; }
message QSize   { int32 width = 1; int32 height = 2; }
message QSizeF  { double width = 1; double height = 2; }
message QPointF { double x = 1; double y = 2; }
message QVector3D { float x = 1; float y = 2; float z = 3; }
message QQuaternion { float x = 1; float y = 2; float z = 3; float scalar = 4; }
```

### File Version

Every entity file contains a `FileVersion` header:

```protobuf
message FileVersion {
    optional int32 version = 1;          // Schema version number
    optional string cavewhereVersion = 2; // App version string (e.g., "2026.4")
}
```

### Units

```protobuf
message Units {
    enum LengthUnit {
        Inches = 0; Feet = 1; Yards = 2; Meters = 3;
        Millimeters = 4; Centimeters = 5; Kilometers = 6;
        LengthUnitless = 7; Miles = 8;
    }
    enum ImageResolutionUnit {
        DotsPerInch = 0; DotsPerCentimeter = 1; DotsPerMeter = 2;
    }
}
```

---

## Entity Identity

Every entity (cave, trip, chunk, note, station, shot, lead, team member, scrap) has a
**UUID** stored in its `id` field. IDs are:

- Assigned once at creation and never change
- Stable across renames, moves, and sync operations
- Used internally for change tracking and conflict resolution

On load, missing or duplicate IDs are detected and repaired:
- Missing IDs are auto-generated
- Duplicate IDs are regenerated for the conflicting entities
- Repairs trigger an automatic save after load completes

---

## Name Sanitization

Entity names are sanitized for use as filesystem directory/file names using these rules:

1. **Forbidden characters** `\ / : * ? " < > |` are replaced with `_`
2. **Leading dots** are stripped (prevents hidden files on Unix)
3. **Trailing dots** are stripped (problematic on Windows)
4. **Leading/trailing whitespace** is trimmed
5. If the result is empty, it becomes `"untitled"`

Name collisions (where two siblings produce the same sanitized name) are detected and
rejected at the UI level. On load, collisions from external edits are repaired with
numeric suffixes.

---

## Bundle Format (.cw)

A `.cw` bundle is a **ZIP archive** containing a complete directory project.

### Creating a Bundle

1. All pending file writes are flushed to disk
2. All changes are committed to Git
3. The project root is zipped using DEFLATE compression
4. Excluded from the archive:
   - Files matching patterns in `.git/info/exclude` (typically `.cw_cache/` and `.DS_Store`)
   - macOS metadata files (`__MACOSX/`, `._*` prefixed files)
   - **The `.git/` directory IS included** to preserve history for sync/merge
5. The archive is written atomically: write to `.cw.tmp`, then rename

### Opening a Bundle

1. The archive is scanned for a `.cwproj` file using `findProjectFileInArchive()`
2. Contents are extracted to a temporary directory (including the `.git/` directory)
3. The project is loaded as a normal directory project (using the existing Git repository)
4. The project is marked as `LoadedFromBundledArchive`

### Re-saving a Bundle

After modifications, the user must explicitly save to update the `.cw` file. The save
process re-zips the working directory to the original archive path.

### Distinguishing Bundle from Legacy SQLite

Both use the `.cw` extension. Detection logic:
1. Try to find a `.cwproj` entry inside the ZIP -> **Bundle format**
2. Try to open as SQLite and check for `ObjectData` table -> **Legacy SQLite** (auto-converts)
3. Neither -> **Unknown/invalid**

---

## Save Process

CaveWhere uses a **queue-based incremental save** system. Changes to individual entities
are saved independently as they occur, not as a monolithic project write.

### Signal-Driven Saving

When a data model property changes (e.g., cave name, shot distance), the corresponding
C++ object emits a signal. `cwSaveLoad` connects to these signals and queues file write jobs.

### Job Queue

Each save operation is represented as a `Job` with:
- **Object ID**: The source object
- **Kind**: `File` or `Directory`
- **Action**: `WriteFile`, `CopyFile`, `Move`, `Remove`, `EnsureDir`
- **Payload**: The protobuf message to serialize (for writes)

Jobs are executed in **FIFO order** from the queue as they are enqueued.

### Optimizations

The job queue applies three compression rules:

1. **Last Write Wins**: Multiple writes to the same file before flush keep only the final version
2. **Write-then-Remove Cancels**: Adding then removing an entity before flush produces no disk I/O
3. **Sequential Moves Collapse**: Renaming A->B->C before flush creates only the final directory C

### Atomic Writes

All file writes use `QSaveFile` (write to temporary file, then atomic rename). This prevents
data corruption from crashes mid-write.

### Flush and Commit

An explicit flush-and-commit operation:
1. Processes all pending jobs
2. Writes files to disk
3. Creates a Git commit with message `"Automatic commit at <timestamp>"`
4. Emits `saveFlushCompleted()` signal

---

## Load Process

### Directory Loading

1. Read the `.cwproj` file to get project metadata and `dataRoot`
2. Scan `<dataRoot>/` for `*.cwcave` files (one per cave)
3. For each cave, scan `<caveName>/trips/` for trip subdirectories
4. For each trip, read the `*.cwtrip` file
5. For each trip, scan `notes/` for `*.cwnote` and `*.cwnote3d` files
6. Parse each JSON file into its protobuf message type
7. Convert protobuf messages to C++ data structures
8. Run identity repair (fix missing/duplicate UUIDs)
9. Run name collision repair
10. Files are loaded in path-sorted order for deterministic results

### Deserialization

JSON is parsed with `google::protobuf::util::JsonStringToMessage()`.

---

## Version Compatibility

### Version History

| Version | App Release | Notes |
|---------|-------------|-------|
| 1-5 | pre-release | Binary protobuf in SQLite; not loadable in current versions |
| 6 | 2025.3 | Last SQLite-based version (auto-converted to v8+ on load) |
| 7 | - | Transitional (not loadable) |
| 8 | 2026-dev | First directory-based version |
| 9 | 2026.4 | Fixed proto field name typos |

### Rules

- **Minimum loadable version**: 8 (versions 7 and below are rejected; version 6 SQLite
  files are auto-converted)
- **Forward compatibility**: If any loaded entity has a `fileVersion.version` greater than
  the application's current proto version:
  - A warning is shown to the user
  - **Saving is disabled** to prevent silent data loss from unknown fields
  - The user must upgrade CaveWhere
- **Backward compatibility**: All version 8+ files load in any newer version. Unknown
  JSON fields are ignored during deserialization.
- **Per-entity versioning**: Each file has its own `FileVersion`. A project can contain
  a mix of entity versions (e.g., after a partial sync from a newer client).

### Legacy Fields

Fields prefixed with `legacy_` exist only for reading old binary protobuf data (version 5/6
SQLite blobs). They are never written in version 8+. When a field is permanently removed,
its number is marked `reserved` to prevent accidental reuse.

---

## Git Integration

### Repository Structure

- Every directory project is a Git repository
- `.git/info/exclude` contains local exclusion patterns (e.g., `.cw_cache/`)
- `.gitattributes` configures Git LFS tracking for binary assets

### LFS (Large File Storage)

Binary files (images, 3D models) are tracked via Git LFS. The LFS-tracked extension list
is built dynamically from all formats supported by `QImageReader` (platform-dependent,
typically includes `png`, `jpg`, `jpeg`, `tiff`, `bmp`, `gif`, `webp`, etc.) plus
`svg`, `pdf`, and `glb`. LFS pointer files are stored in Git; actual binary content lives
on the LFS server.

### Automatic Commits

After a save flush, all modified files in the working tree are committed atomically.
Commit messages are auto-generated with timestamps.

### Sync

The sync operation performs: pull (with rebase/merge) -> reconcile in-memory model ->
push. Merge conflicts in JSON protobuf files are resolved through the reconciliation layer,
not Git's text merge.

---

## Example: Minimal Project

A project with one cave, one trip, and one note:

**`My Project.cwproj`**
```json
{
  "fileVersion": { "version": 9, "cavewhereVersion": "2026.4" },
  "name": "My Project",
  "metadata": { "dataRoot": "My_Project" },
  "id": "a1b2c3d4-e5f6-7890-abcd-ef1234567890"
}
```

**`My_Project/Big Cave/Big Cave.cwcave`**
```json
{
  "fileVersion": { "version": 9, "cavewhereVersion": "2026.4" },
  "name": "Big Cave",
  "id": "11111111-2222-3333-4444-555555555555",
  "lengthUnit": "Feet",
  "depthUnit": "Feet"
}
```

**`My_Project/Big Cave/trips/Day 1/Day 1.cwtrip`**
```json
{
  "fileVersion": { "version": 9, "cavewhereVersion": "2026.4" },
  "name": "Day 1",
  "id": "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee",
  "date": { "year": 2026, "month": 3, "day": 15 },
  "tripCalibration": {
    "distanceUnit": "Feet",
    "frontSights": true,
    "backSights": false,
    "declination": 5.5
  },
  "chunks": [
    {
      "id": "cccccccc-dddd-eeee-ffff-000000000000",
      "leg": [
        { "id": "s1", "name": "A1", "left": "3", "right": "5", "up": "10", "down": "2" },
        { "id": "h1", "includeDistance": true, "distance": "25.5", "compass": "180", "clino": "-5" },
        { "id": "s2", "name": "A2", "left": "4", "right": "6", "up": "8", "down": "1" }
      ]
    }
  ],
  "team": {
    "teamMembers": [
      { "id": "t1", "name": "Alice", "jobs": ["point"] },
      { "id": "t2", "name": "Bob", "jobs": ["sketch", "instruments"] }
    ]
  }
}
```

**`My_Project/Big Cave/trips/Day 1/notes/Survey Scan.cwnote`**
```json
{
  "fileVersion": { "version": 9, "cavewhereVersion": "2026.4" },
  "id": "nnnnnnnn-nnnn-nnnn-nnnn-nnnnnnnnnnnn",
  "name": "Survey Scan",
  "image": {
    "path": "survey-photo.jpg",
    "size": { "width": 4032, "height": 3024 },
    "dotPerMeter": 3937,
    "imageUnit": "Pixels"
  },
  "rotation": 0,
  "scraps": [
    {
      "id": "ssssssss-ssss-ssss-ssss-ssssssssssss",
      "type": "Plan",
      "outlinePoints": [
        { "x": 100.0, "y": 200.0 },
        { "x": 300.0, "y": 200.0 },
        { "x": 300.0, "y": 400.0 },
        { "x": 100.0, "y": 400.0 }
      ],
      "noteStations": [
        { "id": "ns1", "name": "A1", "positionOnNote": { "x": 150.0, "y": 250.0 } },
        { "id": "ns2", "name": "A2", "positionOnNote": { "x": 280.0, "y": 350.0 } }
      ],
      "noteTransformation": {
        "northUp": 0.0,
        "scaleNumerator": { "value": 1.0, "unit": "Inches" },
        "scaleDenominator": { "value": 100.0, "unit": "Feet" }
      }
    }
  ]
}
```

---

## Reading the Format (Implementation Guide)

To read a CaveWhere project:

1. **Detect format**: Check if file is a ZIP (bundle) or directory. For `.cw` files,
   try ZIP first, then SQLite.
2. **Extract if bundle**: Unzip to a working directory.
3. **Parse `.cwproj`**: Read JSON, deserialize as `CavewhereProto.Project`.
   Extract `metadata.dataRoot`.
4. **Scan for caves**: Recursively scan all subdirectories under `<dataRoot>/`. Each
   directory containing a `.cwcave` file is a cave.
5. **Parse each cave**: Read `.cwcave` JSON as `CavewhereProto.Cave`.
6. **Scan for trips**: List subdirectories under `<cave>/trips/`. Each containing
   a `.cwtrip` file is a trip.
7. **Parse each trip**: Read `.cwtrip` JSON as `CavewhereProto.Trip`.
8. **Scan for notes**: List files in `<trip>/notes/` matching `*.cwnote` and `*.cwnote3d`.
9. **Parse each note**: Read as `CavewhereProto.Note` or `CavewhereProto.NoteLiDAR`.
10. **Resolve images**: Image paths in notes are filenames relative to the `notes/` directory.
11. **Validate IDs**: Check for missing or duplicate UUIDs and repair as needed.

---

## Writing the Format (Implementation Guide)

To write a CaveWhere project:

1. **Create project root** with `.cwproj` file containing `Project` message.
2. **Create `<dataRoot>/`** directory.
3. **For each cave**: Create `<dataRoot>/<sanitized-name>/` and write `.cwcave`.
4. **For each trip**: Create `trips/<sanitized-name>/` under cave dir and write `.cwtrip`.
5. **For each note**: Write `.cwnote` or `.cwnote3d` in `notes/` under trip dir.
6. **Copy images/assets** into the `notes/` directory. Store only the filename in the
   protobuf `Image.path` or `NoteLiDAR.fileName` field.
7. **Assign UUIDs** to all `id` fields at creation time.
8. **Set `FileVersion`** on every entity file with the current proto version and app version.
9. **Use atomic writes** (`QSaveFile` or write-to-temp-then-rename) to prevent corruption.
10. **For bundles**: ZIP the project root **including `.git/`**, excluding only `.cw_cache/`,
    `.DS_Store`, and macOS metadata (`__MACOSX/`, `._*`). The `.git/` directory must be
    included to support sync and merge operations after extraction.
