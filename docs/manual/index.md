# CaveWhere User Manual

Welcome to the CaveWhere user manual. CaveWhere is a desktop app for **cave
mapping**: it turns the hand-drawn sketches — or digital scraps exported as SVG
from apps like TopoDroid — and instrument readings surveyors collect
underground into an accurate, shareable 3D cave map.

New here? Read **[Why CaveWhere](concepts/why-cavewhere.md)** first — it explains
the problems CaveWhere solves — then keep the
**[Glossary](concepts/glossary.md)** handy for survey vocabulary.

> This manual is being written chapter by chapter. Links below point to pages
> that exist today; chapters still in progress are listed in plain text so you
> can see the planned structure. (For AI agents: [`llms.txt`](llms.txt) is the
> machine-readable index of available pages.)

## Available now

### Concepts
- [Why CaveWhere](concepts/why-cavewhere.md) — the problems CaveWhere solves.
- [Glossary](concepts/glossary.md) — survey and CaveWhere terms.

### 3D View
- [The 3D View](view-3d/the-3d-view.md) — navigate the model, aim the camera,
  and filter with layers.

### Survey Data
- [Organize Caves and Trips](survey-data/caves-and-trips.md) — the region → cave
  → trip structure, and why the trip is the unit that matters.
- [Enter Survey Data](survey-data/enter-survey-data.md) — the survey table,
  data blocks, and the keyboard shortcuts that make entry fast.
- [Calibrate the Instruments](survey-data/calibration.md) — tape, compass and
  clino corrections, foresights and backsights, and units.
- [Set the Declination](survey-data/declination.md) — magnetic north to true
  north, automatically or by hand.
- [Fix Survey Errors](survey-data/survey-errors.md) — what each error and
  warning means, and how to clear it.

### Notes
- [Add Notes to a Trip](notes/add-a-note.md) — import scans, PDFs, and 3D
  scans, and rotate or remove them.
- [Set the Image Resolution](notes/note-resolution.md) — a note's DPI, and the
  "check DPI" errors it explains.
- [Work with LiDAR Notes](notes/lidar-notes.md) — import a `.glb` scan and set
  its up, north, and scale.

### Scraps and Carpeting
- [Scraps and Carpeting](scraps/carpeting.md) — what a scrap is and how
  carpeting morphs a sketch onto the 3D survey.
- [Digitize a Scrap](scraps/digitize-a-scrap.md) — trace an outline, place
  stations, and mark leads in Carpet mode.
- [Choose the Scrap Type](scraps/scrap-types.md) — plan, running profile, or
  projected profile, plus north/up, scale, and azimuth.
- [Troubleshoot the Carpet](scraps/troubleshoot-carpeting.md) — find and fix a
  distorted carpet.
- [Tune the Warping Settings](scraps/warping-settings.md) — grid, interpolation,
  stations, and smoothing.

## Planned chapters

These directories exist and will be filled in as the manual is built out. Each
chapter is task-oriented and problem-first (see
[the authoring guide](AUTHORING.md)).

- **Concepts** — the data model (Region → Cave → Trip → Survey chunk →
  Shot/Note → Scrap), coordinate systems.
- **Getting Started** — download, first-run identity, a tour of the UI,
  responsive layout.
- **Projects & Files** — new/open/save; the `.cw` legacy, bundled `.cw`, and
  `.cwproj` formats; recent projects; save on exit.
- **Notes (more)** — note keywords build on the [Notes](notes/add-a-note.md)
  pages above.
- **3D View (more)** — field of view, labels, leads, and rendering settings
  (MSAA, EDL) build on [The 3D View](view-3d/the-3d-view.md).
- **Georeferencing** — fixing stations, coordinate systems, grid convergence.
- **Point Clouds** — adding LAZ/LAS, coordinate systems, clipping, EDL.
- **Measurement** — distance, azimuth (grid / true / magnetic), inclination.
- **Loop Closure** — closure errors and reviewing blunders.
- **Leads** — the lead list, editing, CSV export.
- **Import/Export** — Survex, Compass, Walls, and CSV import; export; map
  layout to PNG/PDF/SVG.
- **Collaboration** — why Git and `.cwproj`, GitHub sign-in, the Sync button and
  health, remotes, history and diffs, sharing and deep links.
- **Settings** — Jobs, Warping, PDF/SVG, Git, Appearance, Rendering.
- **Appendix** — small screens, keyboard shortcuts, troubleshooting.
