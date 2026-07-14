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
- **Survey Data** — regions and caves, trips, the shot table, calibration
  (tape / front sight / back sight / declination), team, units, survey errors.
- **Notes** — importing images and PDFs, georeferencing a note (DPI, scale,
  north, up), stations and leads on notes, LiDAR notes.
- **Scraps** — the sketch tool, outlines, scrap types (plan / running profile /
  projected profile), tying stations, warping settings, computing scraps, the
  carpet view.
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
- **Settings** — Jobs, Warping, PDF/SVG, Git, Appearance, Rendering, Sketch.
- **Appendix** — small screens, keyboard shortcuts, troubleshooting.
