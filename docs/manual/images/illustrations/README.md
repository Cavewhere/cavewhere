# Hand-authored illustrations

Unlike everything in `docs/manual/images/`, these files are **not generated** by
`scripts/gen-manual-screenshots.sh` and must not be deleted by it. They are
committed by hand.

They illustrate cave-surveying concepts rather than the CaveWhere UI, so no
screenshot harness can produce them, and no UI change makes them stale. See the
"Images and screenshots" section of `../../AUTHORING.md` for the rule.

## Provenance

| Files | Source |
|---|---|
| `projection-*.png`, `projection-*.jpg` | [Cave Mapping Sketch Projections](https://cavewhere.com/2020/12/15/cave-mapping-sketch-projections/) (cavewhere.com, 2020-12-15) — the project's own blog. Used in `scraps/scrap-types.md` to show one passage drawn as a plan, a projected profile, a running profile, and a cross-section. Renamed to kebab-case; otherwise unmodified. |
| `calibration-reading-direction.svg` | Hand-authored for `survey-data/calibration.md` — one shot read from both ends (60° foresight / 240° backsight), plus the four column-and-reading cases and which box each needs. Drawn from the export math in `cwSurvexExporterTripTask::writeCalibrations`, which adds **−180** to the compass calibration when the sight is corrected/backwards and applies **scale −1** to the clino. Same palette and plain hand-written SVG as `setting-*.svg`. |
| `utm-grid-convergence.svg` | Hand-authored for `georeferencing/grid-convergence.md`, **based on "UTM Grid North Explained" by Mike Futrell** (used with credit). His worked example: two points at longitude 108°E (A at 28°N, B at 33°N) sit on one true-north meridian, yet land on different UTM eastings because grid north leans from true north — the grid convergence. Redrawn in the manual's palette (orange `#d6552b` = true-north meridians, blue `#17618f` = the UTM grid) on a white card. The convergence angle is drawn **exaggerated for legibility**, so this is a schematic, not a to-scale figure. Plain hand-written SVG; credit Mike Futrell here and in the page caption if redrawn. |
| `setting-*.svg` | Hand-authored for `scraps/warping-settings.md` — one diagram per warping setting, drawn from the behaviour in `cavewherelib/src/cwTriangulateTask.{h,cpp}`. Plain hand-written SVG, no editor or toolchain: edit the XML directly. Colours are the manual's own tokens (ink `#171b20`, accent `#17618f`, mark `#d6552b`, muted `#5c656e`) on a white card, matching the `projection-*` figures and staying legible in either page theme. |

### Keeping the `setting-*.svg` figures honest

They depict real algorithms, so their geometry is derived, not eyeballed — if you
redraw them, keep it that way:

- `setting-max-closest-stations.svg` — the candidates are control points every
  0.5 m along the shots (14 px == 0.5 m), because `findClosestInterpolatedStations`
  picks from `buildStationsWithInterpolatedShots`'s list, which is mostly *dummy*
  stations. Drawing them as a scatter of surveyed stations misrepresents the
  geometry. The chosen sets are the true N nearest to the marked point: Max = 3
  lands on 3 interpolated points (span 1.0 m), Max = 10 on 9 interpolated plus the
  surveyed A2 (span 4.5 m ≈ 5 m of line).
- `setting-smoothing-radius.svg` — the smoothed path is a genuine Gaussian-weighted
  average of the raw path (`sigma = radius / 2`, hard cutoff at the radius), matching
  `smoothZ()`. The bell's height at the cutoff is ~14%, not zero, because the real
  filter truncates rather than fades.
- `calibration-reading-direction.svg` — the plan is drawn to the bearing it
  quotes: `A1 (140,192)` → `A2 (370,60)` is `atan2(230, 132)` = 60° from north,
  so the 60°/240° labels describe the geometry rather than sitting beside an
  arbitrary line. The figure deliberately shows only the *direction* each box
  resolves to, not the arithmetic: CaveWhere subtracts 180° either way, so a
  corrected backsight of 60° goes to −120°, which is 240° only after wrapping —
  writing "60 − 180 = 240" in the figure would be wrong.

When adding an illustration, add a row here naming where it came from and which
page uses it, and size it to fit a **512×420** box first (`sips -z <h> <w>`) so
it renders at a similar size to the generated screenshots — see the sizing note
in `../../AUTHORING.md`. The `projection-*` files were fit to that box from the
blog's 1024px-tall originals.

**Reverting generated screenshots will not touch this directory — but only if you
exclude it explicitly.** This directory lives *under* `docs/manual/images/`, so
anything scoped to that path reverts *this README and any illustration edits*
along with the re-rendered PNGs, silently undoing hand-authored work. That trap
catches `git checkout -- docs/manual/images/` and equally
`git diff --name-only -- docs/manual/images/ | xargs git checkout --`. Exclude
this directory by name:

```bash
git diff --name-only -- docs/manual/images ':!docs/manual/images/illustrations' \
  | xargs -r git checkout --
```

(A `*.png` glob dodges the README by luck, but breaks as soon as a new, untracked
screenshot exists — the shell hands git a path it doesn't know and it refuses the
whole command.)
