# th2cwnote

Converts xtherion `.th2` sketch files into CaveWhere `.cwnote` files inside a
`.cwproj` trip tree, so the scanned notes carpet in the 3D view. It is a
stopgap until CaveWhere imports Therion projects itself.

```sh
python3 -m venv venv && venv/bin/pip install -r requirements.txt
venv/bin/python th2cwnote.py TH2_DIR TRIPS_DIR SURVEY.svx            # dry run, prints the report
venv/bin/python th2cwnote.py TH2_DIR TRIPS_DIR SURVEY.svx --write    # writes notes
venv/bin/python preview.py TH2_DIR SURVEY.svx OUT_DIR [FILE.th2 ...] # outlines drawn on the scans
```

- `TH2_DIR` holds the `.th2` files; each `xth_me_image_insert` names the scan
  relative to that directory. Only `.png` scans are used.
- `TRIPS_DIR` is the project's `trips/` directory. `tripNNN-*.th2` goes to
  `Trip N/notes/`; the scan is copied beside the `.cwnote`.
- `SURVEY.svx` supplies the survey legs: station names are checked against it,
  and legs between a scrap's stations join the scrap's outline pieces.

Run with CaveWhere closed when using `--write`.

## What it does

- `th2parse.py` reads scraps, wall lines (Bezier curves sampled), stations,
  `point continuation`, `point label` and the `xth_me_area_adjust` size.
- `registration.py` scores the ink under the drawn walls. When the scan was
  rotated after drawing (area size is the scan's transpose) or re-cropped, it
  searches rotations, scale and shift and keeps a fit that beats the as-drawn
  placement.
- `chain.py` + `outline.py` build the scrap outline the way
  `cwSketchScrapOutlineDetector` does: wall polylines are chained end to end
  into rings, the rings are polygonized, and the faces are outset by `--margin`
  pixels. The margin hugs the walls where a passage folds back on itself, and
  closing joins pieces only when the outline is still in pieces. A scrap with
  no walls falls back to a concave hull (`--method hull`, `--ratio`).
- Leads: every `point continuation` becomes a lead, described by the nearest
  lead-like label within 150 px; other lead-like labels are leads of their own.
  A label saying `TT` (too tight) is a checked dead end and is left out.

The report lists every file, re-registration, dropped station, label kept out
of the leads, and the count of scraps written.
