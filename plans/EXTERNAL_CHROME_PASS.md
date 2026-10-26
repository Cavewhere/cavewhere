# External centerline UI — chrome pass (owner's manual pass 3, 2026-10-14)

Follow-ups from the owner's hand check of the `externalFiles` branch after
MP.1–MP.10 (`plans/SURVEY_TREE_PLAN.html`, "MP — Owner's manual pass"). The
theme: generated UIs are noisy. Every item here removes or merges a readout
unless it says otherwise. Smaller items first, then the two TreeView items.

Pipeline: the Workflow C shape (investigate → failing tests → implement →
simplify → review → fix+commit), one commit per item. UX-only items keep the
investigate stage short (read the files named below, confirm the anchors,
list the tests that pin the removed text). Owner's data
(`/Users/cave/Desktop/OMEGA2`, `/Users/cave/Desktop/survex/houping`) is
private: never a fixture, never read by an agent. Synthetic fixtures live
under `testcases/datasets/external-centerlines/`.

Status: ◻ open · ✅ landed (hash) · ✗ dropped

## Items

- **XC.1** ✅ 59f3a0c6 UX — Fix Stations page node rows show both a `Chip` ("0 fixes")
  and a "No fixed stations" label (`FixStationPage.qml` ~L1264–1274). Keep one
  element: the chip reads "No fixed stations" when `ownFixCount === 0`, the
  label goes. Tests: `tst_FixStationPage.qml` rows pinning either text.

- **XC.2** ✅ fa4c0095 — a Survex file's `*declination auto` on an attached trip. MP.8
  (8990cb6b) computes IGRF at the file's fix on the trip's date with the
  caption "file auto". Owner's rule (2026-10-14): the Decl cell reports the
  computed number, `file auto f.ff` (caption, then the value to two decimals
  with the degree sign, e.g. "file auto 7.50°"), never the caption alone.
  Verify on a dated synthetic fixture under `survex_declination/` (add one if
  missing) and fix the cell if it hides the number. **Owner decision
  2026-10-14 (A):** one rule for every origin, native trips included —
  caption, then the value to two decimals: `file auto -4.63°`, `file -2.50°`,
  `none 0.00°`, `manual 0.00°`, `auto 3.25°` — in the one shared
  `DeclinationReadout.qml` (tree cell and trip page).

- **XC.3** ✅ 69b07657 UX — external trip page cleanup
  (`ExternalCenterlineScopeHeader.qml`, `ExternalCenterlineSolveStatus.qml`,
  `ExternalCenterlineTripPanel.qml`):
  - remove "Part of:" (`ExternalCenterlineScopeHeader.qml` L60) — the
    breadcrumb already says it;
  - remove "Remove trip…" (`ExternalCenterlineScopeHeader.qml` L81–111, with
    its RemoveAskBox positioning) — an external trip is removed with its file,
    never on its own; drop the verb from the trip's row menu too if it is there;
  - remove "View cavern output" (`ExternalCenterlineSolveStatus.qml` L61) — it
    opens the whole project's output, which says nothing about this trip.
  Tests: `tst_ExternalCenterlinePanelComponents.qml`,
  `tst_ExternalCenterlineTripPanel.qml`.
  **Owner decisions 2026-10-14:** (1) remove the whole scope header
  (`ExternalCenterlineScopeHeader.qml` deleted) and the solve status from the
  external trip page too — a trip page shows neither; the source-root page
  keeps its solve status and its "View cavern output" link. (2) Sourced data
  is all or nothing: the tree's Delete… (hide) verb goes from derived nodes
  and derived trips; only the source root can be deleted. The hide/exclusion/
  Restore… machinery it fed is retired in XC.11.

- **XC.4** ✅ ca1daf56 UX — remove the "%n warning(s)" count and its colored badge from
  `ExternalCenterlineSolveStatus.qml` (after XC.3 its only host is
  `SourceRootCard.qml`; `ErrorIconBar.qml` L151 is the row badge's own text,
  keep it). The row's ErrorIconBar and the node warnings banner already carry
  the warnings. Drop `cwLinePlotManager::lastSolveWarningCount` if nothing
  else reads it.

- **XC.5** ◻ **document only — no design work yet.** Replace the external
  trip page's Team table and station data with a scoped view of the external
  text file that highlights the trip's lines
  (`ExternalCenterlineTripMetadata.qml` L94 `TeamTable`, the survey editor
  the trip page loads for an external trip). Design when the owner asks.

- **XC.6** ✅ 4a3e64ed UX — Cave/Node page: remove the "Open %1" link
  (`DerivedNodeCard.qml` L61) and the "This node is %1 in the project" line
  (`DerivedNodeCard.qml` L72). The breadcrumb and the Kind chip already carry
  both. Tests: `tst_NodePage.qml`.

- **XC.7** ✅ 1c762da9 — the Cave page reports "Declination comes from the cave's
  survey file." (`ExternalCenterlineTripMetadata.qml` L85–92
  `caveOwnsDeclinationHint`; also L75–83 `fileOwnsDeclinationHint`). Report
  the number instead: the trip's applied declination with MP.8's caption
  ("file", "file auto", "none"), read-only. Reuse the tree's Decl cell text
  rule so the page and the tree agree. Tests: `tst_CavePageExternalCave.qml`,
  `tst_ExternalCenterlineTripPanel.qml`.

- **XC.8** ◻ audit — sweep the external-centerline branch for redundant
  chrome the items above miss: `ExternalCenterline*.qml`, `DerivedNodeCard.qml`,
  `SourceRootCard.qml`, `NodePage.qml`, `FixStationPage.qml`,
  `SurveyTreeRow.qml`. Rule: a fact shown once per page; a readout the
  breadcrumb, Kind chip or a row badge already gives is removed. Runs last and
  is report-only: it lists candidates with file:line for the owner to pick;
  nothing is implemented until the owner answers.

- **XC.9** ✅ 889195a5 — Expand all / Collapse all for the survey TreeView
  (`SurveyTreeView.qml`, used by `DataMainPage.qml` and `NodePage.qml`), the
  same single toggling icon button FX.8 gave the Fix Stations page
  (`FixStationPage.qml` L1057 `expandAll`/`collapseAll`, `hasCollapsedNodes`).
  Extract a shared QML button (e.g. `ExpandCollapseButton.qml` in
  `cavewherelib/qml`) and use it on both pages; the Fix Stations page loses
  its local copy. Tests: `tst_DataMainPageTree.qml`, `tst_NodePage.qml`,
  `tst_FixStationPage.qml`.

- **XC.10** ✅ 77435bcc — resizable columns in the survey TreeView
  (`SurveyTreeView.qml` L1010 `columnWidthProvider`) on the Cave page and the
  Data page, the way FX.13 did it for the Fix Stations page
  (`FixStationPage.qml` L1084 `resizable: true`, L1109). Share the header
  delegate if FX.13's is page-specific. Width reset stays filed as FX.21.
  Tests: `tst_DataMainPageTree.qml`.

- **XC.11** ✅ 7f9dd918 — retire the derived hide: with XC.3 the only entry point to
  `RootData.hideDerived`, the per-root exclusion list that survives Reload,
  and the root page's Restore… is gone. Delete the verb, the exclusion
  persistence (proto field — unshipped, so delete and renumber, no
  migration), the Restore… UI, and their tests; a saved project carrying
  exclusions loads with every derived row visible. Supersedes
  `SURVEY_TREE_PLAN.html` L219's hide design.

- **XC.13** ✅ 88cee3c9 — the external trip page's Date is editable
  (`ExternalCenterlineTripMetadata.qml` L42 `DoubleClickTextInput`
  `tripMetadataDate`, with the L13–17 comment citing §5 Q6). **Owner
  decision 2026-10-15:** read-only. The file owns the date the way it owns
  the team: show it as a plain label, delete the edit path (and the §5 Q6
  rationale in the comment). Tests: `tst_ExternalCenterlinePanelComponents.qml`
  L651 `test_tripMetadataDateAndDeclination`, L795.

- **XC.14** ✅ 6f0ebeb4 (survex 9c97bfd2, dewalls c5b53c3) bug — a Walls project whose `.wpj` carries a `.REF` line with
  an empty datum name (`… 15 ""`, Walls writing the datum by its index only)
  refuses to attach: `dewalls/src/wallsprojectparser.cpp` L394
  `expect(QRegularExpression("[^\"]+"))` throws on `""`, `parseFile` returns a
  null root, `scanWallsFile` (`cwExternalCenterlineScanner.cpp` L1874) records
  "could not parse Walls project" and leaves `dependencies` empty, and
  `scanWithEntry` (L2349) reports the misleading "scanWalls: entry file could
  not be read". Found on a private owner project (never a fixture): a 51-book
  `.wpj` whose other five `.REF` lines name `"NAD83"` / `"NAD27 CONUS"` with
  indexes 19 / 15. Fix: the parser accepts an empty datum name; the scanner
  resolves the datum from `wallsDatumIndex` when the name is empty (Walls'
  datum table — the file itself pairs 15 with NAD27 CONUS and 19 with NAD83;
  `cwWallsImporter.cpp` L77 `wallsDatum(datumName)` is the name path). Also
  make the failure honest: when the project file parses with an error, the
  scan's error is the parser's message (file, line), never "entry file could
  not be read". Synthetic fixture under `walls_empty_datum/` (a two-survey
  `.wpj` with a `""` datum `.REF` plus its `.srv`s). Tests:
  `test_cwExternalCenterlineScanner.cpp` (scan succeeds, coordinate system
  resolved from the index; a truly malformed `.wpj` reports the parse
  message); a dewalls parser test if the submodule has a suite the build runs.
  After it lands the owner's project is re-tried by hand (by the main agent,
  outside the workflow) for any further failure.
  **Owner decision 2026-10-15 (B, upstream-shaped):** cavern rejects the
  empty name too (`survex/src/datain.c` `case WALLS_WPJ_CMD_REF`, "Datum “”
  not supported"), so this item opens the survex fork for one change shaped
  as a patch for upstream: when the quoted name is empty, resolve the datum
  from the index. The Walls source (checked 2026-10-15, frozen from the 2014
  initial commit through v2.2.5-beta.1) makes the index the datum:
  `Walls32/datum.cpp:30-135` `gDatum[]`, 0-based — 0 Adindan, 1 Arc 1950,
  2 Arc 1960, 3 Australian Geod \`66, 4 Australian Geod \`84, 5 Camp Area
  Astro, 6 Cape, 7 European 1950, 8 European 1979, 9 Geodetic Datum \`49,
  10 Hong Kong 1963, 11 Hu-Tzu-Shan, 12 Indian Bangladesh, 13 NAD27 Alaska,
  14 NAD27 Canada, 15 NAD27 CONUS, 16 NAD27 Cuba, 17 NAD27 Mexico, 18 NAD27
  San Salvador, 19 NAD83, 20 Oman, 21 Ord Srvy Grt Britn, 22 Prov So Amrican
  \`56, 23 Puerto Rico, 24 South American \`69, 25 Tokyo, 26 WGS 1972,
  27 WGS 1984; legacy codes 254 = NAD27 CONUS, any other 251–255 = WGS 1984
  (`Walls32/Mainfrm.cpp:1243-1245`). Walls writes the in-memory name
  verbatim and fills an empty one from the index only lazily
  (`Prjdoc.cpp:627-644`, `Mainfrm.cpp:1262-1269`), so `""` is ordinary Walls
  output. A name that is present but unknown keeps today's error
  (`tests/wallsbaddatum.wpj` keeps its meaning). The same table goes into
  dewalls for the scanner and importer. Follow-up after landing: a PR to
  upstream survex with a synthetic `wallsrefindex.wpj` test and the Walls
  citations (also mention Walls writes elevation with `%u`, so a negative
  elevation arrives as 4294967xxx).

## XC.8 audit result (2026-10-14, HEAD 7f9dd918, report only)

Agent recommendations; the owner picks. Numbers are the pick list.

Remove:
1. `NodePage.qml` L404 and L824 (narrow): "Fixed positions come from the survey file" — the lock Kind chip says it; hide the L803 "·" for derived nodes.
2. `DerivedNodeCard.qml` L46: format label — Kind chip says it; card then holds the file name only.
3. `ExternalCenterlineAttachedHeader.qml` L136: format label beside the file name — Kind chip + extension.
4. `FixStationPage.qml` L1757 (narrow): "from <file>" — the wide row uses the lock alone.
5. `ExternalCenterlineFilePicker.qml` L128: "(auto-detected)" suffix.
6. `AttachSurveyFileDialog.qml` L266: block-preview hint sentence (`previewHintText()`) — the preview below shows it.
7. `ReplaceCenterlineDialog.qml` L81: "Pick the replacement file:".

Merge:
8. `SurveyTreeRow.qml` L436: ExternalSolveBadge's amber warning dot beside ErrorIconBar's warning → keep the badge's red error dot only, fold the cavern warning count into ErrorIconBar.
9. `FixStationPage.qml` L888: "Coordinate system for bare fixes: unset (set on X)" is the third readout of the unset state → show the row only when a system is set.
10. Attach dialog: format + file count shown by the picker (L124/L13x) and by `scanSummaryText` (L223) → picker hides its summary there.
11. `AddSurveyFileDialog.qml` L167: "CaveWhere stays in sync" is stale → match the Attach dialog's "Reload brings the changes in".

**Owner picks 2026-10-15:** 1 keep; 2, 4, 5, 6, 7 remove; 8, 9, 11 yes; 10 yes
(see 6); 3 remove (decided 2026-10-15: the extension carries the format; the source-root page also has the Kind chip) — XC.12f. For 6 the owner widened it: the whole preview
block goes (`blockPreview`: title "Subtree CaveWhere will create", hint,
list) and the "Place under: Region root (top level)" row too; with it the
dialog's own `scanSummaryText` goes, so the picker's "Format: Survex" /
"✓ Found N file(s)" lines are the one summary (10). Implemented as XC.12:
one commit per group — a) 2 ✅ c24e072c (DerivedNodeCard.qml folded into NodePage), b) 4 ✅ ad5360ac, c) 5+6+7+10+11 ✅ f479cf17 (also drops cwScanRow's QML registration; the Attach dialog shows no survey/station counts now), d) 8 ✅ 475b086f (read warnings become a banner/ErrorIconBar row; a missing *include shows red badge + warning), e) 9 ✅ 6c29c405 (owner check: with no frame and no system the unset badges and the descendant up-arrow link are hidden too), f) 3 ✅ 280b2fa2 (cwExternalCenterline no longer exposes `format` to QML; the scanner's formatName stays for the picker).

Keep (flagged): native page-header KindIcon (`NodePage.qml` L251); row KindIcon beside the Kind chip (`SurveyTreeRow.qml` L446, W2 design); root "%1 stations" status; Replace's "Currently attached:"; "Copied from:"; the "Stations" header (XC.5 replaces the list).

## Order

XC.1–XC.11 landed 59f3a0c6..7f9dd918 (2026-10-14); XC.8 audit reported above, implementation after the owner picks.
XC.12a–f landed c24e072c..280b2fa2 (2026-10-15). Suites at 280b2fa2: C++ 2805/2805, QML 1401/1401.
XC.13 88cee3c9, survex upstream-main merge 65b04407 (survex c8ac6934), XC.14 6f0ebeb4 (2026-10-15). The owner's CKKC project solves with the patched cavern (32,110 stations, 0 errors, 42 project-own warnings). Upstream patch exported to the scratchpad (`upstream-patch/0001-Resolve-an-empty-Walls-.REF-datum-name-from-its-inde.patch`, applies cleanly to survex/main) — PR to upstream pending the owner's go. Suites at 6f0ebeb4: QML 1401/1401; C++ 2806/2809 under load (both suites concurrent, load 9–12) — the three (`test_cwScrapManager.cpp:1313` progress steps, `test_cwSurveyNodeSyncMergeHandler.cpp:135` clone wait, `test_cwTriangulateTask.cpp:150` crop bounds) pass together alone, 57/57. Walls fix-location investigation filed as FX.22 in `FIX_STATION_CS_AND_TREE_PLAN.md`; FX.22 80582256, FX.23 477dfee9 (survex e4b27048), FX.17 0e83a44a landed 2026-10-15/19. Suites at 0e83a44a: C++ 2825/2825; QML 1399/1401 under load (load 15–19) — `TripSync::test_tripAddLiDARNoteSyncAndCheckout` passes alone 22/22; `NarrowToolbar::test_prevNextNavigation` (note-gallery prev/next, untouched by these commits) passes alone 2 of 3 runs at load 18, a load flake to watch. TV.1 3fc9bd83 (wide node page tree virtualizes) landed 2026-10-19; suites at 3fc9bd83: C++ 2825/2825, QML 1403/1403. TV.2 ef9e4618 (tree scrollbar) landed 2026-10-20; suites at ef9e4618: QML 1406/1406; C++ 2828/2830 under load (load 15) — the same `test_cwTriangulateTask.cpp:150` and `test_cwScrapManager.cpp:1313` as at 6f0ebeb4, both pass alone 52/52. FX.25 66dbd244 (detached Walls entries skipped) landed 2026-10-20, after the owner's 508307d3; suites at 66dbd244: QML 1411/1411; C++ 2832/2833 under load (load 11–14) — `test_cwGeometryItersecter_twoLevel.cpp:119` 10 s async-build timeout, `[twoLevel]` passes alone 3 of 3 runs (20/20 each). FX.24 11a08eb6 (+ test follow-ups 0fc715cc, translated() unit tests 010b192c) (bare fixes on a local grid) landed 2026-10-20; suites at 11a08eb6: QML 1411/1412 — `tst_ExternalSolveBadge` read-warning count, a real consequence (603 now joins the read warning for a frameless bare-fix file), test updated to pin both warnings (uncommitted); C++ 2843/2846 under load — `test_cwLinePlotTask_AttachInclude.cpp:334` collision test was timing-sensitive under FX.24 (raw x=100 before the scan lands, translated x=0 after), now waits for LocalGrid and expects the translated geometry with the leg lengths as the separation proof (uncommitted, `[AttachInclude]` 3 of 3 alone); `test_cwSurveyImportManager.cpp:39` (Compass badFile error count) and `test_cwTriangulateTask.cpp:150` pass alone, load flakes.
XC.5 waits for the owner.
Owner manual check of CKKC at 0fc715cc (2026-10-21): FX.25 + FX.24 pass.
origin/dev (fc7d50bd) merged into externalFiles as 061606cf (2026-10-21): loop errors travel per owning node (splitLegErrorsByNode); suites green except the two point-cloud tests dev already fixed elsewhere.
origin/dev (f159ed3a) merged as 07066e85 (2026-10-21): dev's point-cloud test fixes, LAZ vertical unit, declination elevation; the two point-cloud tests pass again.

## Open from earlier passes (still filed, not here)

FX.15–FX.21 (`plans/FIX_STATION_CS_AND_TREE_PLAN.md`), CP.2d+, FX.3 (#708),
MP.11 (`wulong_north` main-thread ≤16 ms, worker + progress), the MP.8/MP.9
follow-ups recorded in `plans/SURVEY_TREE_PLAN.html`.
