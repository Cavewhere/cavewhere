# Fix stations from external files: coordinate system choice and the subtree Fix Stations page

Branch `externalFiles`. Sits under SURVEY_TREE_PLAN.html → Pre-W8 (after CP.1 `854d424b`, before CP.2 and W8).
Prototype the owner approved: `plans/FIX_STATION_TREE_UX_INTERACTIVE.html`, **TreeView layout**.

Four items, run strictly in order, one workflow stage set per item
(implement → simplify → review → fix+commit, one commit each). Every item starts
with failing tests written by a repro agent from a **synthetic** fixture under
`testcases/datasets/external-centerlines/`. Private data on the owner's Desktop
is never read and never copied.

## 0. Background a sub-agent needs

### The bug (owner-reported)

A Survex file with `*fix` lines and no `*cs` ("bare fixes") solves in vanilla
cavern on its own, and fails as soon as any `*cs out` is in force above it:
cavern error 438 *"The output projection is set but the input projection
isn't"* (`survex/src/commands.c` ~L1276, read-only). CaveWhere puts a `*cs out`
in force whenever the project is georeferenced (`cwSurvexExporterRegion.cpp`
L35–52), and today hides the error by also emitting `*cs <project frame>`
before the `*include` (`cwSurvexExporterCaveTask::writeIncludeSystem`,
`cwSurvexExporterCaveTask.cpp` L677–694) — so bare numbers are read in the
project's frame, silently, with only a soft warning
(`cwLinePlotManager::publishAttachedFixWarnings`, L906–957).

Owner decisions:

1. **Default (a):** with no user choice, CaveWhere emits nothing for bare fixes;
   cavern reports error 438 on those lines; the owner node gets a typed
   warning badge. The silent project-frame fallback is removed.
2. The user chooses one coordinate system **per attachment** ("Read fixes
   with no coordinate system as X"). It applies to every bare fix in that
   attachment's file subtree. Fixes with their own `*cs` keep it.
3. Children show the inherited X read-only and link up to the owner.
4. The Fix Stations page becomes a **TreeView** over the node and its subtree;
   the root's page shows every fix in the project. Attached rows are shown
   under the derived node whose file holds the fix.

### Where things live today

| Thing | File | Notes |
|---|---|---|
| Scan of `*cs` / `*fix` | `cavewherelib/src/cwExternalCenterlineScanner.cpp` L361–377 regexes, L926–943 handling, L794–804 `recordFix` | coordinate text stored verbatim |
| `ScannedFix` | `cwExternalCenterlineScanner.h` L296–310 | `station, coordinate, coordinateSystem, file, line; hasCoordinateSystem()` |
| Compass `.mak` fixes | scanner L1145–1160 | system = `.mak` datum/zone, NAD27 default, else system in force |
| Walls `#FIX` | scanner L1305–1325, L1513–1525 | system = enclosing `.REF`, else empty |
| Fixes keyed per owner | `cwExternalCenterlineManager.cpp` L758–768 (`result.ownerFixes`), L931–957 `refreshAttachedFixes`, L969–979 `readInSystemFor` | owners = nodes/trips with a non-empty `externalCenterline` (`collectOwnerSnapshots` L585–615); derived nodes are never owners |
| Attached row model | `cwAttachedFixModel.{h,cpp}` | read-only, `ReadOnlyRole`, `ReadInWarningRole`; concatenated after the node's own rows by `cwSurveyNode::fixStationTable()` (`cwSurveyNode.h` L239–245) |
| `cwAttachedFix` value type | `cwExternalCenterline.h` L77–116 | QML value type |
| Attach record | `cwExternalCenterline` (`cwExternalCenterline.h`), proto `ExternalCenterline` (`cavewhere.proto` L127–131: `entry_file` only) | setters on `cwSurveyNode::setExternalCenterline`, `cwTrip::setExternalCenterline`; serialized in `cwSaveLoad.cpp` |
| Driver include | `cwSurvexExporterCaveTask.cpp` L296–325 (root path), L406–409 (trip path), L677–694 `writeIncludeSystem`; `cwSurvexExporterUtils.cpp` `CsScope::ensure/ensureAnySystem/writeFileOutputSystem`; `cwSurvexCS::toSurvexCS` (`cwSurvexCS.cpp` L306–338) | bare names inline, EPSG via sidecar |
| Typed warnings → badges | `cwError.h` L36–38 (`600..602`), `cwLinePlotManager.cpp` ~L980 `setTypedWarning(AttachedFileDefaultDatum…)`, `cwNodeWarningModel.cpp` L204 click-through to the Fix Stations page | add `603` the same way |
| Fix Stations page | `cavewherelib/qml/FixStationPage.qml` (813 lines; `CSCell` L305, `ReadOnlyLock` L264, `TableStaticColumnModel` L381, `tableView`) | one node's flat table |
| Survey tree model/view precedent | `cwSurveyTreeModel.{h,cpp}`, `SurveyTreeView.qml`, `SurveyTreeRow.qml`, `RegionSurveyTree.qml` | `QAbstractItemModel` + `QQ.TreeView`, chosen in W2 |
| Derived node identity | `cwSurveyNode::sourcePath()` (`cwSurveyNode.h` L80, L129–137); scanner `NodeRow.sourcePath` (`cwExternalCenterlineScanner.h` L130–176), `childNodeRow` | maps a scanned file/block to its derived node |

Tests that already cover this area: `testcases/test_cwAttachedFixModel.cpp`,
`test_cwExternalCenterlineAttachCave.cpp`, `test_cwExternalCenterlineAttachSubtree.cpp`,
`test_cwLinePlotTask_AttachInclude.cpp`, `test_cwNodeWarningModel.cpp`,
`test-qml/tst_FixStationPage.qml`, `tst_CavernOutputPageExternal.qml`.

### Rules for every agent

- Build only: `/Users/cave/Qt/Tools/CMake/CMake.app/Contents/bin/cmake --build build/Qt_6_11_1_for_macOS-Debug --target cavewhere-test cavewhere-qml-test CaveWhere`. Never reconfigure.
- Tests: `ASAN_OPTIONS=detect_container_overflow=0 ./build/Qt_6_11_1_for_macOS-Debug/cavewhere-test "<tags>" 2>&1 | tee <scratchpad>/<name>-$$.log`; QML `cavewhere-qml-test --platform offscreen -input test-qml/tst_X.qml`. Run only the item's gate inside a workflow; full suites run once, outside, after the pipeline.
- `git --no-optional-locks status`; stage explicit paths; never `plans/`, `QuickQanava/`, `survex/`; never push; never amend or revert.
- The `survex/` submodule stays untouched (FX.3 names the one exception, owner-gated).
- Repository CLAUDE.md conventions. No objectName on a layout container. No `*ForTesting` API. American English, facts stated positively.
- A workflow agent never ends its turn with a question.

## FX.1 — The Survex `reference` flag leaves the coordinate text (LANDED 4be54dda)

**Problem.** `*fix <station> reference x y z` is a cavern keyword
(`commands.c` L1183–1188: suppresses the unused-fix warning). The scanner
stores `"reference 0779271 3271580 843"` as the coordinate and the row shows it.

**Change.**
- `ScannedFix` gains `bool reference`; `scanSurvexFile` strips a leading
  `reference` token (case-insensitive, the way cavern matches it) before
  `recordFix`. Compass/Walls leave it `false`.
- `cwAttachedFix` gains `bool reference` (QML `MEMBER`), filled in
  `refreshAttachedFixes`.
- `cwAttachedFixModel` gains `ReferenceRole`; the Fix Stations row shows a small
  "reference" chip (Theme tokens) on such rows. Tooltip: "The file marks this
  fix as a reference point; cavern keeps it even when no leg uses it."

**Tests.** Fixture `survex_reference_fix/` (one `*fix A reference 10 20 30`,
one plain `*fix B 1 2 3`, one leg). C++: scan yields coordinate `"10 20 30"`
with `reference == true` for A, `false` for B; model role matches. QML: chip
visible on A's row only.

**Gate.** `cavewhere-test "[ExternalCenterlineScanner]" "[AttachedFixModel]"`; `tst_FixStationPage.qml`.

## FX.2 — A per-attachment coordinate system for bare fixes, default (a) (LANDED ddceb5ab; 599 retired in favor of 603; Replace keeps the choice; 603 lists station names)

**Data.**
- `cavewhere.proto` `ExternalCenterline`: `optional string fix_coordinate_system = 2;`
  (unshipped proto: plain add, no migration).
- `cwExternalCenterline`: `fixCoordinateSystem` (`Q_PROPERTY … WRITE … FINAL`,
  same string form the project's own fix rows use with `cwSurvexCS`), in
  `operator==`, save/load in `cwSaveLoad.cpp`, attach-dialog defaults it empty,
  Reload keeps it (it lives on the record, which Reload preserves).
- Setting it from QML goes through the owner's `setExternalCenterline` (value
  type), which already dirties the project and triggers a re-solve.

**Driver.** `writeIncludeSystem(stream, ownerId, …)` receives the owner's
`fixCoordinateSystem`:
- non-empty → `scope.ensure(toSurvexCS(choice))` before the `*include`
  (sidecar for EPSG, as native fixes do);
- empty → emit no input `*cs` for the include. Keep `*cs out` handling as is.
  Remove the project-frame fallback and `readInSystemFor`'s prose for it.

**Warning / badge.** New `cwErrorTypeId::AttachedFileFixWithoutCS = 603`.
`publishAttachedFixWarnings` sets it on the owner node (and trip owners' node)
when the attachment has ≥1 bare fix and `fixCoordinateSystem` is empty **and**
the project is georeferenced: `"%1 fixes %n station(s) without a coordinate
system; choose one on the Fix Stations page or add *cs to the file."` Clear it
when a system is chosen. `cwNodeWarningModel` routes 603 to the Fix Stations
page like 602. When the project has no frame and no file sets `*cs out`, bare
fixes are legal (vanilla behavior): no warning.

**Rows.** `cwAttachedFix.readInSystem` becomes the owner's choice (empty =
unset). The CS cell on a bare attached row shows a warning badge `none` then
`→ X` or `→ unset` (danger), tooltip per the prototype. Rows stay read-only.

**Owner-page control (interim, moves into the tree in FX.4).** Above the table
on the attachment owner's Fix Stations page, visible only when the node (or one
of its trips) has an attachment with ≥1 bare fix:
`Read fixes with no coordinate system as [ combo ]` — the same CS combo the
native rows use, plus an "unset" entry. One control per attachment owner on
the page (node + each attached trip).

**Tests.** Fixture `survex_bare_fix/`: a bare-fix file (`*fix A 500000
3000000 100`, two legs, no `*cs` anywhere). The test project supplies the
`*cs out` by being georeferenced. Cases:
1. Georeferenced project, no choice → solve fails, first error line contains
   "output projection is set but the input projection isn't", owner node has
   warning 603, badge visible, row shows `→ unset`.
2. Choose `UTM48N` → driver text contains `*cs UTM48N` before the `*include`,
   solve succeeds, station lands at the expected UTM coordinate, warning gone,
   row shows `→ UTM48N`, save/load round-trips the choice, Reload keeps it.
3. Project with no frame, bare file → solves as before, no warning.
4. A trip-level attachment gets its own control and its own choice.
5. Compass `.mak` naming a zone and Walls with `.REF`: unchanged
   (regression tags `[Compass]`/`[Walls]` attach tests).

**Gate.** `cavewhere-test "[AttachInclude]" "[Attach]" "[ExternalCenterline]" "[cwNodeWarningModel]" "[SaveLoad]"` (plus the repro tag); `tst_FixStationPage.qml`, `tst_CavernOutputPageExternal.qml`, `tst_SourceRootPage.qml`.

## FX.3 — Cavern survives a coordinate system PROJ cannot build (DEFERRED: GitHub #708, Summer 2026 milestone; skip in this pipeline)

**Finding.** `cmd_fix` (`commands.c` L1234–1255) calls
`proj_create_crs_to_crs` and dereferences the result without a null check;
when PROJ finds no `proj.db` (standalone binary without `PROJ_DATA`, or a
mis-deployed app bundle) the solve thread crashes instead of reporting.

**Options for the owner to choose before this item runs:**
- (i) Null check in `survex/src/commands.c` → `compile_diagnostic(DIAG_ERR, /*Failed to convert coordinates*/436)` and return. This is a change inside the submodule: separate commit in `survex/`, then a pointer bump in the superproject. Allowed only when the owner says so in the workflow brief.
- (ii) CaveWhere-side: before any cavern run, `cwCoordinateTransform` verifies every CS string the driver will emit (project frame, each fix's CS, each attachment choice) resolves, and the solve reports a readable error listing the offending system. Also assert the PROJ search path is set before the first `cwCavernRunner::run`.

Recommended: both — (i) is the real guard, (ii) is the readable message. Tests: (ii) a unit test with an unresolvable system string → error names it, no crash; (i) is covered by running the bundled cavern on a driver whose `*cs` is garbage with `PROJ_DATA` unset, expecting exit code ≠ 0 and no signal.

## FX.4 — Fix Stations page as a TreeView over the subtree (CP.3) (LANDED f76cc7fe; region root gets Data › Fix Stations; node chip counts unset fixes via SubtreeUnsetFixCountRole; firstFixWithoutCoordinateSystem renamed firstFixWithoutReadInSystem)

**Re-home attached rows.** `refreshAttachedFixes` stops putting every row on
the owner. For each `ScannedFix`, find the derived node whose `sourcePath`
matches the fix's file/block (use the scanner's `NodeRow.sourcePath`; the
scanner already pairs files to rows via `childNodeRow`). Rows whose file maps
to no derived node (the entry file itself, a hidden/excluded path) stay on the
owner. `cwSurveyNode::attachedFixes` thus becomes "fixes held in this node's
own file(s)". Warning 603 and the `.mak` datum warning stay on the owner (they
describe the attachment), and `cwNodeWarningModel` click-through opens the
owner's page, where the tree shows the rows below.

**Model.** New `cwFixStationTreeModel : QAbstractItemModel`
(`QML_NAMED_ELEMENT(FixStationTreeModel)`, property `rootNode`): the root row is
`rootNode`, child rows are its child nodes (depth-first, every node including
ones with zero fixes), and leaf rows under each node are that node's
`fixStationTable()` rows (native rows first, then attached). Node rows expose:
name, kind (`cwSurveyNodeKind`), own fix count, subtree fix count, subtree
bare-fix count, `isAttachmentOwner`, `fixCoordinateSystem` (owner's choice,
writable), `inheritedFixCoordinateSystem` + `inheritedFromNodeId` for
descendants of an owner, and the node id for navigation. Leaf rows forward
the underlying table's roles (so `FixStationPage`'s existing delegate
components bind unchanged) and forward `setData` to the native model. Keep it
live: rebuild the affected branch on `fixStationTable` row changes,
`attachedFixesChanged`, node add/remove/move, `externalCenterlineChanged`.
Follow `cwSurveyTreeModel` for the node-walk and change-propagation pattern.

**View.** `FixStationPage.qml` replaces the flat `tableView` with a
`QQ.TreeView` (same pattern as `SurveyTreeView.qml`): node rows carry chevron,
kind chip, name, count chip, badges, and, on an attachment owner, the
`Read fixes with no coordinate system as [combo]` control (moved here from
FX.2); descendant rows show `Coordinate system for bare fixes: X (set on
<owner>) ↑` with ↑ navigating to the owner. Leaf rows reuse the existing
cells; indent only the station cell so numeric and CS columns line up across
depths (the prototype's rule). "Expand all / Collapse all" buttons. Everything
starts expanded. The page's own node is the root row, expanded, with no
collapse chevron. `addFix()` adds to the page's node (root row) as today;
`pickRow`/`currentFixRow` map to leaf rows.

**Tests.** C++ `[FixStationTreeModel]`: structure for a synthetic project
(native folder + cave, Survex root with two derived files incl. one with no
fixes, Compass `.mak` + `.dat`, Walls `.wpj` + `.srv`): row counts per node,
re-homed attached rows under the right derived node, counts and bare counts
per branch, choice writable on owner rows and readable on descendants, live
update when a native fix is added and when an attachment is detached. QML
`tst_FixStationPage.qml`: tree renders, collapse hides leaves, native cell edit
still commits, attached row locked, owner control changes descendants'
`→ X`, "Open page"/↑ navigation, root page lists every fix in the project.
Keep every existing `tst_FixStationPage` case green (edit/delete/pick-from-view
work on the root row's own leaves).

**Driver edge carried from FX.2.** A native fix written in the same block
before the `*include` leaves its `*cs` in scope, so cavern reads the file's
bare fixes in that system while warning 603 still says none was chosen. End
the native fixes' CS scope before `writeIncludeSystem` (a `CsScope` nested
for the native fixes, or an explicit reset) so an unset choice always means
"no input system at the include". Test: native fix with `EPSG:32648` on the
owner + bare file + no choice → error 438, warning 603.

**Gate.** `cavewhere-test "[FixStationTreeModel]" "[AttachedFixModel]" "[Attach]" "[cwNodeWarningModel]" "[BareFixCS]" "[Exporter]"`; `tst_FixStationPage.qml`, `tst_SourceRootPage.qml`, `tst_NodePage.qml`.

## FX.5 — A native node's `*cs` ends with its own fixes (LANDED ea924c51; shield = a native node whose subtree reaches an unset bare-fix include ends every input `*cs` it writes, fixes and `*declination auto` alike; the `[Attach][Trip][CS]` test now expects 438; `[NativeCSScope]` pins the shape)

**Problem.** A `*cs` opened by an enclosing native node's fixes, or by a native
node's fixes written ahead of a trip attachment's `*include`, is still in force
at a child source root's `*include`. With no choice made, cavern reads the
file's bare fixes in that system while the page and warning 603 say none was
chosen. Example: cave "Main" with one native fix in EPSG:32648 and a child
source root attached to `survex_bare_fix/bare_fix.svx` with no choice → solve
succeeds, badge says unset. FX.4 closed the same edge for the owner's *own*
native fixes only (`cwSurvexExporterCaveTask.cpp` ~L325).

**Change.** Write every native node's own `*fix` lines inside an anonymous
`*begin`/`*end` whenever they emit a `*cs` (`CsScope` re-emits the system for
later native children that need it), and do the same ahead of a trip
attachment's include, so an unset choice always means no input system at the
include. Keep the driver skeleton tests in
`test_cwSurvexExporterRegion_NodeTree.cpp` as the shape oracle; update expected
lines deliberately.

**Tests.** The scenario above for a child source root and for a trip
attachment under a node with its own fix → error 438 + 603; and a native child
cave after such a wrap still solves with its parent's fix in scope where it
relied on it. Gate: `[Exporter]`, `[BareFixCS]`, `[AttachInclude]`, `[Attach]`.

## FX.6 — Attached `.dat` stations have no name labels in the 3D view (LANDED 23176c2d; cause was format-agnostic: a scan whose re-solve left positions unchanged sent the label view no rebuild signal; new `memberViewsChanged` pulse relayed by the region root)

**Observation.** With a Compass `.dat` attached, the line plot renders but the
3D view shows no station name labels for its stations. Native stations and
stations from an attached Survex file show theirs.

**Investigate before changing anything.** Suspects, in order: the post-solve
naming path (`cwScopeLabels::resolve` + `cwTrip::scopePrefix`, the only
sanctioned naming path per CLAUDE.md) for a derived `.dat` trip; the
`cwCavernNaming` decode of the per-DAT survey names the survex port emits (each
`.mak` DAT gets its own survey, so `.dat` labels may carry a prefix the decode
treats as foreign); and whether the label layer is fed by the solved network at
all for Compass-attached trips. Reproduce with a **synthetic** `.dat` fixture
under `testcases/datasets/external-centerlines/` (private data under
`~/Desktop` is never a fixture). Then file the fix as its own item with a
failing test first.

## FX.7–FX.14 — Owner's manual pass over the fix station tree (2026-10-12; ALL LANDED via Workflow C: FX.7 7415bd43 — ~205 ms → ~15 ms per toggle pair, `reuseItems` back on; FX.8–10 85eca33e one commit; FX.11 d29c81c7 — `.mak` fixes in meters, numbers only; FX.12 c0936ed6 `cwFixStationTreeFilterModel`; FX.13 44e2e456 widths in QSettings per page; FX.14 53c4fe4a — a second unset bare-fix attachment now fails 438 + 603 instead of riding the first's frame)

Each is its own pipeline item with a failing test first unless marked UX-only.

**FX.7 — Expanding and collapsing is slow.** Verified shape: the page is one
`QQ.TreeView` over `cwFixStationTreeModel` (a `QAbstractItemModel`) with a
`DelegateChooser`; there is no ListView inside a delegate, and that stays the
rule (all rows in the model, the view reads it directly). Suspects, in order:
`reuseItems: false` on the view, so every expand instantiates a fresh copy of
the heavy fix-row delegate (editors, pickers, badges) for each visible row;
`toggleNode` → `applyExpansion` re-walks the whole subtree and calls
`expand()` per node row (a relayout each) after every single toggle;
`onWidthChanged: forceLayout()`. Measure with a synthetic project of ~50 nodes
× ~20 fixes before changing anything; fix: make the pooled-row bug that forced
`reuseItems: false` go away properly (delegates read every value from
`required property`/model roles, no cached names), toggle one row without the
full re-walk, and keep the fix row's editors lazy (`Loader`) until focused.

**FX.8 — Expand all / Collapse all → one icon button (UX).** Replace the two
text buttons with a single icon button that toggles between expand and
collapse, placed top right on the same line as **+ Add Fix**. A right-click
menu on node rows (Expand all / Collapse all under this node) is the
alternative the owner also accepts.

**FX.9 — Remove "Open Page"; the node name is the link (UX).** Drop the
Open Page affordance from node rows; the cave/node name is a link to its page,
matching the rest of the app's page links.

**FX.10 — Drop the "n with children" tag (UX).** Node rows show the own
count only; the subtree count role stays in the model for the root page.

**FX.11 — Compass `.mak` fix rows show an `f` in the coordinate text.** A
`.mak` fix line carries unit/flag tokens (`[f,...]`) and the coordinate
column shows one of them. Reproduce with a **synthetic** `.mak` under
`testcases/datasets/external-centerlines/` (the owner saw it on private data,
which is never a fixture); the parser should drop the token and the row show
numbers only. Related to C4d.4's `.mak` fix handling.

**FX.12 — Sort and filter on the tree's columns.** A `QSortFilterProxyModel`
over `cwFixStationTreeModel` (recursive filtering on) with a filter field and
sortable headers; node rows keep their place under their parent.

**FX.13 — Resizable column widths.** The header columns get drag handles and
the widths persist per page in `QSettings` (see how `TableStaticHeaderColumn`
is used elsewhere before adding a new mechanism).

**FX.14 — Choosing a bare-fix system leaves the project without one.** After
picking a system for an attached Survex file's bare fixes, the project still
has no coordinate system, so the solved plot is off-center in the 3D view.
Native fixes seed the project frame (`cwGeoReference` /
`cwLocalProjectionManager`); the attachment's choice should seed it the same
way when the project has none, and the Fix Stations page should say so.
Test: attach `survex_bare_fix/bare_fix.svx`, set the system, assert the
project frame is set and the plot is centered.

## Order and bookkeeping

1. Repro agent (Opus): FX.1 + FX.2 fixtures and failing tests → report.
2. Workflow A: FX.1, FX.2 sequential.
3. FX.3 deferred to GitHub #708 (Summer 2026).
4. FX.4 landed f76cc7fe (Workflow B).
4b. Workflow C (2026-10-12, 6 stages per item: investigate → failing tests → implement → simplify → review → fix+commit) landed FX.5, FX.6, FX.11, FX.14, FX.7, FX.8–10, FX.12, FX.13 as ea924c51..44e2e456.
5. Full C++ and QML suites once after each workflow, outside it. After Workflow C at 44e2e456: QML 1372 passed, 0 failed, 88 skipped; C++ 2 failures in `test_cwLinePlotTask_NodeTree.cpp` (the Dome climb attachment never chose a system; FX.5's shield now gives 438 there, as designed) → fixture chooses `kFrameCS`, `[NodeTree]` green, edit UNCOMMITTED pending the owner.
6. Plan bookkeeping done in `SURVEY_TREE_PLAN.html`; then CP.2 (hang investigation), then W8.

## FX.15+ — Follow-ups filed by Workflow C's agents (OPEN, unscheduled)

- **FX.15** A file with `*cs` but no `*cs out` fails its harvest with cavern 437 (the harvest passes an output system only when the scan finds a bare fix, `cwExternalCenterlineManager.cpp` ~L710), so its stations go unharvested and FX.5's shield skips it. Likely a real bug; the `own_cs_fix.svx` fixture needed its own `*cs out` because of it. **FX.15 ✅ 909c3c8a** (2026-10-21, workflow `wf_010465de-a4a`): the harvest driver now writes `*cs out X` and `*cs X` for any Survex closure that fixes a station, X = the file's own `*cs out`, else the first own-cs fix's system (a geographic one becomes a WGS84 UTM EPSG code); bare-only files unchanged; `bareFixSystem` and `hasFixWithoutCoordinateSystem` deleted; `own_cs_fix.svx` lost its workaround `*cs out`; fixture `survex_own_cs_no_out/`, tag `[OwnCsHarvest]`. Three existing fixtures (survex_subtree_fixes/ridge.svx, fix_tree_sort_filter/sorted.svx, survex_block_rows/order.svx) now harvest where they failed silently. Open: a `*cs` naming a `.prj` the temp driver cannot find; a top-level `*cs` with no `*fix` ("Couldn't read the station names"); the closure is scanned twice per harvest (accepted).
- **FX.16** Units in attached fix rows: attached rows show bare meters whatever the project units are, while native rows show project units with a suffix; a Survex `*fix` under `*units feet` shows feet numbers unlabeled; a Walls `#FIX` with a unit suffix (`1234.5f`) or under `#UNITS feet` still shows its text as written — if the owner's `f` was on a Walls row, FX.11 did not cover it. **FX.16 ✅ d6926031** (2026-10-21): cavern applies no `*units` to `*fix`, so the scanner is unchanged; attached rows expose numeric components (`cwExternalCenterline::gridPosition`, `cwAttachedFixModel`) and render through the native formatter in project units; the editor still offers the text as written. Behavior changes: coordinate sort now mixes attached rows with native ones by number (rows in different systems compare raw); the cell no longer shows Survex standard errors or the file's separators. Limit shared with native rows: PROJ `+vunits` is ignored.
- **FX.17** Project frame seeding gaps (from FX.14): files that name their own system (`*cs`, Compass `.mak`) still give a frameless project no frame; "Recenter on data middle" ignores attached fixes; a loaded Attachment anchor whose owner is gone keeps its frame with an empty description; re-evaluation on each frame change walks all nodes twice per node whose attached list changes (O(nodes²)).
- **FX.18** New Project keeps the root node's own fix stations: `cwCavingRegion::clearCaves` clears only child nodes. **FX.18 ✅ 5cb13231** (2026-10-21): `clearCaves` also clears the root's trips and own fix stations (no undo command for the fix clear). Found while doing it, OPEN: (A) root fix stations are never persisted — no `cwCavingRegionData`/proto field, lost on reopen; (B) root fixes are never solved, validated or frame-seeded, since every consumer walks `rootNode()->allNodes()` which omits the root, so Add Fix on the region page creates a row that changes nothing. Follow-up: persist root fixes and include the root in those walks, or hide Add Fix on the region row. A sync full reload (setData) now drops unsaved root fixes too, as a reopen already did.
- **FX.19** Filter awareness (from FX.12): node count chips, the tree's count and the empty-state box ignore the filter; no test drives the bare-fix system picker's `setData` through the proxy; coordinate sort is stale after an edit that changes only northing or elevation (accepted). **FX.19 ✅ 18a13593** (2026-10-21): visible counts come from the proxy (`visibleFixCount`/`visibleFixRowCount`), chips read "V of N fixes" under a filter, the empty state names the filter; picker `setData` through the proxy is tested; stale coordinate sort stays accepted.
- **FX.20** Tree-model cleanups (from FX.7): `fixRowsMoved` still emits a per-row `FixRowRole` change that only a test asserts; `collapsedNodeIds` keeps stale ids after a node is deleted (one click clears it); the CS combo is instantiated at rest rather than lazily. **FX.20 ✅ 0a52b75d** (2026-10-22): (1) the per-holder `FixRowRole` announcement is KEPT — both row delegates bind `fixRow`, and it is the only refresh for rows below a collapsed node (no view layout happens); three tests pin it; (2) stale `collapsedNodeIds` pruned one tick after rowsRemoved/modelReset against the source model (a moved node keeps its state; undo brings a pruned node back expanded); (3) the input-CS picker loads on edit: at rest the cell shows the system name as plain `Theme.text` (owner: no link color, no icon), double-click opens the picker in place, single click selects the row, Escape closes it, it also closes when rows shift or another row is picked; one open picker per page. Owner manual check pending: double-click opens, single click selects, Escape restores. Known: a mouse double-click is only testable as a touch double-tap headless.
- **FX.21** Column widths (from FX.13): no reset-to-default control; defaults scale with `fontScale`. **FX.21 ✅ 1e33946f** (2026-10-22): a context menu on the column header (right-click / long-press, wide layout only) resets the three widths by writing 0 to their QSettings keys (QML Settings has no remove; 0 is the page's default sentinel). Defaults already scale with the font through FX.13's Theme tokens; a test now pins it. `Theme.tightSpacing` (2 px) stays fixed.
- **FX.22** Walls `#FIX` locations never reach the project frame (investigated 2026-10-15 against the owner's private Walls project; synthetic fixtures only). Two layers, both in CaveWhere — cavern places the stations correctly (the `.3d` is `CS EPSG:26916` with 30+ `FIXED` nodes at true UTM):
  1. `cwLocalProjectionManager::gatherAttachedFixInputs` (L242) skips every attached fix that carries its own `coordinateSystem`. A `.REF` gives every Walls fix one (`EPSG:269xx`), so a Walls project seeds no frame: the region stays Ungeoreferenced, the solved plot sits at UTM magnitudes in floats (the existing `walls_empty_datum` solve test already says "positions are floats at UTM magnitudes" and uses a 0.5 m margin). This is the concrete case of FX.17's first bullet, and the same gap covers Survex `*cs` and Compass `.mak` files.
  2. `cwAttachedFix::placementIn` reads the row's text as "easting northing elevation, meters". A Walls fix line is read by cavern under the file's `#UNITS`: `ORDER=NEU`/`NE` swaps the first two numbers, `FEET` (or an `F` suffix) scales them, lat/long is written `W97:43:52.5 N31:16:45`, a `(R5,?)` variance and a `/note` may follow. The scanner stores the text after the station name verbatim (`collectWallsStations` L1688), so for the owner's active fix files (`#UNITS Meters ORDER=NEU`, elevations like `766.19F`, `/Historic…` notes) the parse fails on `F`; an `ORDER=NE` line with plain numbers parses swapped and the domain check then drops it silently. Fixing 1 alone would therefore still leave these fixes unplaced.
  Also seen, and correct as is: cavern drops a fixed station no leg touches ("Unused control point", 14 in the owner's project), so a `Flag=FIX` control book of unsurveyed benchmarks lists rows with no station in the plot. Proposed shape: the scanner resolves each Walls fix to cavern's reading (order, length unit, suffix, lat/long → a canonical "E N U m" or "lat long" text, variance and note stripped) so `placementIn` and the row stay honest; and the frame seeding takes attached fixes with their own system as inputs in that system (FX.17). FX.16 (unit display) is the row-side counterpart.
  Investigation (2026-10-15, workflow `fix-pass-fx22-fx17`) added: the System column holds a display name ("NAD27 CONUS, UTM zone 16N") that `placementIn` hands to PROJ, and PROJ's name lookup rejects "NAD27 CONUS, …" and every Compass "North American 19xx, …" name, so those rows never placed even with clean numbers. **Owner decision 2026-10-15: option C** — the row shows the name and the PROJ code, grid "<datum>, UTM zone <z> (EPSG:<rect>)", lat/long "<datum>, latitude/longitude (EPSG:<geo>)", Compass rows "<name> (EPSG:<code>)" alike; the scan carries the PROJ code in its own placement-system field (filled for Compass too) and `solveSystem()` prefers it. Gate selectors must be one comma-joined Catch2 argument (separate tag arguments AND together). **FX.22 ✅ 80582256** (2026-10-15): scanner reads each `#FIX` through dewalls with the entry's options and `#UNITS` in force; `placementSystem` on `ScannedFix`/`cwAttachedFix` (Walls and Compass); `cwWallsReferenceCS`; fixture `walls_fix_units`; gate 2505/2505 + `tst_FixStationPage` 83/83. Side effects: Compass/Walls rows in a georeferenced project now place, so their nodes gain a grid-convergence readout; lat/long rows show decimal degrees, longitude first; the System column sorts by the full label.
- **FX.23** cavern places a Walls grid `#FIX` raw under any `*cs out` (found by FX.17's investigation 2026-10-15). `datain.c` `WALLS_CMD_FIX`: the lat/long branch converts into `proj_str_out`; the grid branch calls `fix_station` with the file's numbers although the `.REF` set `pcs->proj_str` to the UTM system. With no `*cs out` the `.REF` becomes the output too, so a stand-alone `.wpj` solves right; under CaveWhere's local frame the UTM numbers are written into the frame unconverted (verified: `*cs out <tmerc>` + `*include walls_empty_datum.wpj` puts A1 at 580661.57 4113846.34 in the frame, ~4,100 km off). Same gap today for any Walls grid fix in a project with a native-fix or layer frame. **Owner decision 2026-10-19:** fix it in cavern (fork, upstream-shaped, confined to the grid branch of `WALLS_CMD_FIX` + a cavern test of a `.wpj` included under a `*cs out`), exported as a second upstream patch beside the datum-index one; the driver stays as is and Walls variances carry through. Lands before FX.17 so Walls joins frame seeding without a stopgap exclusion. **FX.23 ✅ 477dfee9** (2026-10-19; survex `e4b27048` on `survexLib`): the grid branch mirrors `cmd_fix` (cached transform, 436/437/438); new cavern tests `wallscsout` (G1 pinned to the `cmd_fix`/cs2cs reference) and `wallscsoutnoref` (438), placed alphabetically so the upstream patch stands alone: `scratchpad/upstream-patch/0001-Reproject-a-Walls-grid-FIX-into-the-output-coordinat.patch`. Interim behavior until FX.24: in a frameless project a `.REF` Walls include sets `proj_str_out`, so a later no-`.REF` Walls grid `#FIX` fails with 438 (include-order dependent). Known upstream quirk left alone: these diagnostics can name the line before the `#FIX`, as 488 already does. The survex commit body uses upstream's British spelling; reword before sending if wanted.
- **FX.24** Bare fixes are a local grid, placed at the origin (owner, 2026-10-19; replaces FX.2's default (a) "error 438 until a system is chosen"). A file whose fixes name no system is a local grid with no position on Earth. CaveWhere picks an origin inside that dataset (its first bare fix) and translates the file so that origin sits at the project's local origin; the file is visible and correctly shaped, just unlocated. Warning 603 stays, reworded to "placed at the project origin until a coordinate system is chosen"; choosing a system (FX.2's picker) moves it to its real place. Mechanism under a frame, no cavern change: the driver writes an input `*cs CUSTOM` that is the frame's own tmerc with `+x_0`/`+y_0` set to the origin fix's numbers, so PROJ applies a pure translation and z passes through untouched. Decisions: (1) a project with no frame translates too — 10⁵-magnitude raw coordinates are a problem, the data must sit near the origin — so the first bare dataset defines the origin, which needs an offset-only, geography-free frame state in `cwGeoReference` (revise its "no separate scene offset" note; the LDP alone cannot cover a project with no geography); (2) a bare file tied to the native survey by an equate is translated anyway and cavern reports the misclosure — its fix is kept; (3) several bare files each center on the origin and overlap, accepted because the overlap shows the data is ungeoreferenced. FX.17's decision A (a file's own `*cs out` read-in) stays: a file that says what its bare numbers are in is a better guess than a translation. FX.23 decision A (cavern 438 for a Walls grid fix under `*cs out` with no input system, mirroring `cmd_fix`) stands; after FX.24 CaveWhere always supplies an input system, so cavern never reaches it from CaveWhere. **FX.24 ✅ 11a08eb6** (2026-10-20, workflow `wf_47b97da0-d35`; 36 files, +1299/−370): `cwLocalProjection::translated(frame, origin)` copies the frame's own conversion and moves only its false easting/northing (EPSG 8806/8807, 8816/8817, 8826/8827 as `kEpsg*` constants) so the origin fix lands on (0,0) — same base CRS, pure translation, z untouched; the driver writes it as the `*cs` before the attachment's `*include` when nothing is chosen and the file names no `*cs out`. New `cwGeoReference::LocalGrid` state (proto `LOCAL_GRID = 3`, `hasCoordinateSystem()` false, stored string empty): the solve runs in the hidden `localGridSystem()` (a tmerc at lat 0 / lon 0 that only carries translations, never shown as geography); a frameless project enters it on its first bare file, a real input takes over via `anchorToFirstUsable` and the bare files keep translating onto the new frame; losing every input returns to LocalGrid. Origin = first parseable bare fix in `m_ownerFixes` scan order. 603 reworded "… placed at the project origin until a coordinate system is chosen"; Fix Stations badge "none → origin". Review red fixed before commit: a stored Frozen frame now waits for its own inputs before a bare file may trade it for LocalGrid (test "A restored frozen frame is kept when a bare attached fix lists before its inputs"). `readInSystemFor` no longer depends on the frame (removed a two-manager ping-pong); the manager re-evaluates queued on `localProjectionChanged`. Fixture `survex_bare_fix_origin/`. Gate 379/379 + five QML files green. **Behavior changes for owner review:** (a) frameless projects with bare files move to LocalGrid and re-save metadata; (b) **undecided by the owner:** a bare fix read in through its file's own `*cs out` is now a frame input, so a frameless project whose only located data is such a file becomes Anchored on it (scene centered there) instead of solving at raw projected numbers — needed because the LocalGrid carrier would otherwise convert those UTM numbers through lat 0 / lon 0; (c) LAZ layers with no CRS load untransformed and no longer line up with a bare file that is now at the origin; the far-from-origin banner is nearly unreachable. **FX.24 test follow-up ✅ 010b192c** (2026-10-21, workflow `wf_4b589967-32f`): `[Translated]` sections in `test_cwLocalProjection.cpp` check a PROJ-string LDP, a WKT NAD83(2011) LDP, raw EPSG:32612, EPSG:2232 (Lambert 2SP, US survey feet, parameters 8826/8827) and EPSG:2056 (Hotine oblique Mercator B, 8816/8817) through PROJ: origin to (0,0), offset point to (dx,dy), z through, datum kept; empty for geographic, garbage, NaN and no-false-origin frames; `localGridSystem()` at lat 0/lon 0. Each test mutation-checked. The feet frame exposed a defect: `translated()` added the origin as meters where its contract takes the frame's axis unit; it now scales by the axis unit factor (1.0 for every meter frame production builds today). Remaining follow-ups: `translated()` returns empty for a frame with no false-origin parameter (438 still reachable then; every CaveWhere-derived frame is a tmerc, so accepted); the AttachInclude trip-section origin check cannot distinguish a leaked native `*cs` when d1 is 0 0 0.
- **FX.17 decisions (owner, 2026-10-19):** runs as written after FX.23, all three formats seed. A Survex file whose own-system fix seeds the frame while its bare fixes rode the file's own `*cs out` (`survex_bare_fix_tree/north/north_cs_out.svx`: `h1` under `*cs UTM48N`, `e1` bare) keeps the read-in while the frame is absent or anchored on an attachment: the driver writes `*cs out <frame>` for the project and an input-only `*cs <file's *cs out>` at that include, the row keeps its "Read in …" note, and the file solves centered (option A; B "error until a system is chosen" and C "such a file does not seed" rejected). Investigation also found: "Recenter on data middle" in an attachment-only project is undone by the next evaluate (Frozen frame drops attached inputs, `clearFrame` runs) — fold the fix into FX.17 (a Frozen frame keeps attached inputs when no native fix or layer input exists). `attachmentDescription` must name a station among own-system fixes. **FX.17 ✅ 0e83a44a** (2026-10-19): own-system fixes are frame inputs placed via `placementIn(solveSystem())`, ordered after bare fixes whose owner has a chosen system, same domain check and Attachment anchor; `forEachAttachedFixInput` is the one walk shared with `attachmentDescription`; a Frozen frame with no native fix or layer keeps attached inputs (Recenter survives); `attachedFixesTakePart()` public with a change signal; decision A read-in writes the file's `*cs out` as an input-only `*cs` at the include. Gate 2147/2147 + five QML files green. Behavior changes to know: more mixed projects reach cavern 438 until FX.24 (an own-system file seeds the frame beside a bare-fix file with no system chosen); `compass_project` now gets `*declination auto`; a stored Frozen attachment-only frame can recenter on load. Still open from the FX.17 list: the O(nodes²) re-evaluation walk; a loaded Attachment anchor whose owner is gone (and the owner-keyed anchor re-anchoring on the owner's first input after a handover).


- **FX.25** The scanner walks detached `.wpj` entries (investigated 2026-10-20 against the owner's private CKKC project, read-only; synthetic fixtures only). Walls `.STATUS` bit 0x2 marks an entry detached ("floating": Walls `prjdoc.cpp` reads `m_bFloating = status & 2`, `m_uFlags = status >> 3`); cavern honors it (`datain.c` `WALLS_WPJ_STATUS_DETACHED`, skipped for the root, which carries the bit too) and solves nothing under it. dewalls' `WpjEntry` marks that bit "?" and `collectWallsSurveys` (cwExternalCenterlineScanner.cpp) filters only `isOther()`, so detached books and surveys become tree rows, their stations count, and their `#FIX`es join `listedAttachedFixes`. The frame then anchors on the first fix in `.wpj` order: in CKKC that is `CSHT1` in the detached "Sheet Layout" book, a map-sheet corner 8.3–10.3 km west of the surveyed entrances (`ENTLIDAR` `C12:A0` through `KhanSfc`), which is the owner's "cave offset 9–10 km". Fix: skip a non-root entry with status bit 0x2 in the scan (and the same check in `cwWallsImporter` if it walks entries itself), matching cavern; CKKC then anchors on `C12:A0`. A stored project keeps its anchor until the detached fix disappears from the inputs, at which point `evaluateFrame` hands over to a nearby input or the first usable one. **FX.25 ✅ 66dbd244** (2026-10-20, workflow `wf_657aed88-3e7`): `kWallsDetachedStatusBit` (0x2 on the raw `WpjEntry::Status`) skips a non-root child in `collectWallsSurveys` and `cwWallsImporter::convertBook`, before `rejectAbsolutePath`, so a detached entry's `.PATH`, `.REF`, file and fixes reach nothing; fixture `walls_detached/` (root `.STATUS 11`, detached book with a far fix, detached survey, attached entrance survey); tests in the scanner, importer and `test_cwLinePlotManager_AttachedCenterlines` (anchors on E1 at the origin). Gate 235/235 + QML 10/10. Behavior to know: detached `.srv` files are no longer copied into the project (`computePlan` mirrors `scan.dependencies`; a reconcile removes existing copies) and edits to them raise no source-change notice; a stored project anchored on a detached fix re-anchors on its next evaluate, so its plot moves. Untested edge: a detached `.SURVEY` outside any book (cavern depth 0 processes it; dewalls' parenting of such an entry is unknown).

- **FX.26** The region root holds no fix stations; Add targets the current row's node (filed 2026-10-24, owner decisions below). Found by FC.1 and pinned as today's reach by the test "a fix on the region root raises no project-wide warning" (`test_cwFixStationValidator.cpp` ~1464): `cwSurveyNode::allNodes()` (`cwSurveyNode.cpp` ~809) returns descendants only, so every project-wide walk written as `rootNode()->allNodes()` skips the root itself — the validator (`cwFixStationValidator.cpp` ~196, ~325, ~454), `cwLinePlotManager::connectNodeInputs` (~360: a root fix edit never re-solves), the error publish walk (~982), `resolveResultsToLive` (~79), and `cwLocalProjectionManager` (~260, ~337, ~377, ~953: a root fix never seeds the frame). Meanwhile FX.4 gave the region root a Fix Stations page whose Add Fix calls `addFixStation()` on the page node's own model (`FixStationPage.qml` `addFix()` ~115, `fixStationsModel: cave.fixStations` ~40), so a user can create exactly the fix nothing judges. A fix belongs to the node whose scope holds its station; the region root has no scope of its own and no coordinate-system choice, so no use case needs one there. Attachments never place anything on the region root: a source root node takes the scanned (read-only) fixes, and a native fix on a source root already anchors a file with no fixes of its own (`test_cwLinePlotManager_AttachedCenterlines.cpp` ~868 solves one). **Owner decisions (2026-10-24):** (1) Add targets the node of the tree's current row — a node row targets itself, a fix row targets the node that owns the fix — and with no current row the page's own node, so a cave page behaves as today. (2) Add is disabled when the target cannot hold native fixes; the region root never can (read-only sourced descendants: the investigation confirms how the model marks them; a source root can). The disabled button carries a tooltip telling the user to select a cave (or folder, or similar) to add a fix station to; the empty-state box on the root page says the same. (3) Button label naming the target ("Add Fix to Upper Level") — owner did not rule; default is to keep "Add Fix" and name the target in the tooltip. (4) Node rows become selectable (currentIndex extends to node rows; the pick-from-view tool and `currentFixRow` treat a node row as "no fix selected") and get a right-click / long-press menu with "Add fix station"; the fix-row `DataRightClickMouseMenu` gains the same item naming the owning node. After an add the tree expands the target and reveals the new row through the existing `revealFixRow` path, filter cleared as today. (5) Load drops any fix stored on the region root; no migration, no proto change (nothing has shipped since 2026.4.3, so the owner does not consider this a leak worth more than the one-line guard). (6) The `allNodes()` walks stay as they are: with the root unable to hold fixes, the validator, frame and re-solve gaps close by construction. The pinning validator test is replaced by one that pins the page's refusal (model-level: the root's fix count stays 0 through the page's add path; QML: Add disabled on the root page with no selection, enabled after selecting a cave row, the new fix lands on that cave). No undo step. Tests: `[cwFixStationTreeModel]`, `[cwFixStationValidator]`, `[cwCavingRegion]` (the two "empties the root's own fix stations" cases stay), `tst_FixStationPage*.qml` (the investigation names the file). Gate: `cavewhere-test "[cwFixStationTreeModel],[cwFixStationValidator],[cwCavingRegion],[SaveLoad]"` plus the Fix Stations page QML test. Size S–M; one commit; runs in its own pipeline or beside the SB.3–SB.6 cleanups.

## TV — Survey tree view follow-ups (filed 2026-10-19)

- **TV.1** The wide node page stutters on a node with 1,000+ trips (owner report on CKKC's Main; the Data page scrolls smoothly). Cause: `NodePage.qml` wide layout sets `scrollable: nodePageArea.isNarrow` and `Layout.preferredHeight: tripTreeId.fullHeight` (`SurveyTreeView.fullHeight` = toolbar + header + `rows * Theme.treeRowHeight`) so the tree sits at its full height inside `wideFlickableId`; a `TreeView` virtualizes only against its own viewport, and `SurveyTreeRow` is a per-cell delegate, so every row × every column is alive. Measured offscreen (1600×900, one cave, 1,200 trips): Data page with the cave expanded keeps 29 Name cells and scrolls at 15.9 ms per 200 px step; the node page holds 1,200 Name cells (×8 columns), is 31,200 px tall, opens in 7.8 s and scrolls at 71.5 ms per step. **Owner decision 2026-10-19: run the recommended fix** — on the wide page the tree scrolls itself (`scrollable: true`) and takes the viewport's remaining height (the page Flickable's height minus what sits above the tree in its column and the page margins, floored at a minimum), the page Flickable stays for a tall stats column, `fullHeight` and its comment go (NodePage is its only user). `tst_NodePage.qml` ~L145 ("the wide node page scrolls the tree with the page") flips to assert the tree scrolls itself; a new test with many trips asserts the live delegate count stays bounded by the viewport. Trade-off accepted: a wheel over the tree scrolls the tree, as on the Data page. **TV.1 ✅ 3fc9bd83** (2026-10-19): wide tree height = page Flickable height − margins − what sits above it in its column, floored at `Theme.treeMinimumHeight`; `fullHeight` and `scrollable` removed from `SurveyTreeView`; `collectByName` moved to `MainWindowTest`; 300-trip scale test bounds the live Name cells by the viewport (and by a quarter of the trip count, so a full-height tree fails it). Gate 159/159 across eight QML files.
- **TV.2** (filed by TV.1's commit notes) Neither page's `TreeView` shows a scrollbar; the wide node page used to inherit the page's, so a long trip list now scrolls with no visible bar. Add `QC.ScrollBar.vertical` to `SurveyTreeView`'s `TreeView` (AsNeeded), which serves the Data page too. **TV.2 ✅ ef9e4618** (2026-10-20): AsNeeded bar on `treeViewId`; while the rows can scroll the Name column gives up the bar's width (`Theme.scrollBarThickness` + 2×`Theme.scrollBarPadding`, 12 px) via `scrollBarGutter` + `forceLayout`, and gets it back when they fit; tests in `tst_NodePage.qml` and `tst_DataMainPageTree.qml` check the Decl edge clears the bar and `position` moves on a wheel. Gate 106/106 over four files. Accepted residual: with the Name column at its minimum in a very narrow window, the fading handle can cover Decl's last 12 px at the far right of a horizontal flick (a `rightMargin` on the view is the follow-up once `HorizontalHeaderView` syncing with a margin is verified).
