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

When adding an illustration, add a row here naming where it came from and which
page uses it, and size it to fit a **512×420** box first (`sips -z <h> <w>`) so
it renders at a similar size to the generated screenshots — see the sizing note
in `../../AUTHORING.md`. The `projection-*` files were fit to that box from the
blog's 1024px-tall originals.
