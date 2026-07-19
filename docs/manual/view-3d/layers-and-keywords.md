---
title: Focus the View with Keyword Layers
summary: Use the 3D view's Layers tab to show only part of a cave by filtering on keywords — narrowing with AND columns that drill down level by level, and combining independent selections with the "Also Include" (OR) button.
problem: A big project draws every cave, trip, scrap, lead, and point cloud at once, which is too much to see or work in; keyword layers let you show exactly the part you care about — one trip, one caver's passage, just the leads, only the plan scraps — and hide the rest.
keywords: [keywords, layers, layer visibility, filter, 3d view, show, hide, and, or, also include, type, cave, trip, caver, year, plan, line plot, lead, laz layer, drill down, focus]
related: [the-3d-view.md, ../concepts/data-model.md, ../survey-data/caves-and-trips.md, ../leads/track-and-export-leads.md, ../point-clouds/add-a-point-cloud.md, ../concepts/glossary.md]
---

# Focus the View with Keyword Layers

## Why / when you need this

The 3D view draws everything in the project at once — every cave, every trip's
survey line, every carpeted scrap, every lead marker, every point cloud. On a
small cave that's exactly what you want. On a real project — several caves, years
of trips, thousands of stations — it's far too much to see through, let alone
work in. When you're checking one team's passage, planning against just the
leads, or lining the survey up against a point cloud, everything else is in the
way.

Keyword layers are how you cut the scene down to the part you care about. They
work like layers in a drawing program — show some things, hide others — except
you don't manage a fixed list of layers by hand. Instead you filter on the
**[keywords](../concepts/glossary.md#keyword)** CaveWhere has already attached to
everything it draws, which lets you carve the scene along whatever line matters
right now: by cave, by trip, by the person who surveyed it, by object type, or by
several of those at once.

## What a keyword is (and where it comes from)

A keyword is a `key = value` tag, like `Type = Plan` or `Caver = Alice`. You
never type these — CaveWhere generates them automatically from your survey data
as it builds the model, and every object inherits the tags of everything it hangs
under. A single plan scrap, for instance, carries its own `Type = Plan` plus the
`Cave`, `Trip`, `Year`, `Date`, and `Caver` tags of the trip and cave it belongs
to, so the *same* scrap can be found by filtering on any of them. (This is the
"keywords cut across the tree" idea from
[How a Project Is Organized](../concepts/data-model.md).)

The keys you'll see most often, and where their values come from:

- **Type** — what kind of object it is: `Plan`, `Running Profile`, and
  `Projected Profile` (the three [scrap](../concepts/glossary.md#scrap) types),
  `Line Plot` (the survey centerline and station labels), `Lead` (unexplored
  [leads](../leads/track-and-export-leads.md)), `LAZ Layer`
  ([point clouds](../point-clouds/add-a-point-cloud.md)), and `LiDAR` (LiDAR
  notes). This is the most useful grouping and the one a new column starts on.
- **Cave** — the cave's name. The line you filter on to isolate one cave in a
  multi-cave project.
- **Trip** — the trip's name, plus **Year** and **Date** from the same trip.
- **Caver** — one tag per team member on the trip. This is why the team list
  matters and why names must be spelled consistently (see
  [Organize Caves and Trips](../survey-data/caves-and-trips.md)): `Phil` and
  `Philip` are two different cavers to the filter, so a typo splits one person's
  passage across two values.

You don't need to memorize the list. Open the filter, pick a key, and CaveWhere
shows you every value it found for it, with a count beside each.

## Open the Layers tab

In the 3D view, the side panel on the right has two tabs, **View** and
**Layers**. Click **Layers**. (On a narrow window the side panel is replaced by a
floating **layers** button — the stacked-sheets icon over the model — which slides
the same panel out as a drawer.)

![The Layers tab of the 3D view's side panel: one bordered filter group whose first column is grouped by Type, listing values such as Line Plot, Plan, Running Profile, and Lead, each with a checkbox and an object count, and an "Also Include" button along the bottom.](../images/view-3d-layers.png)
*The Layers tab. Each bordered box is one filter group; inside it, a column picks
a keyword key (here **Type**) and lists that key's values with counts to check or
uncheck. The **Also Include** button adds another group.*

## Show and hide by one keyword

The panel starts with a single filter group — one bordered box — holding one
column. At the top of the column is a dropdown that picks which **key** to filter
on; it starts on **Type**. Below it is the list of that key's **values**, each a
checkbox followed by a count in parentheses — `Plan (4)`, `Line Plot (2)`,
`Lead (2)` — telling you how many objects carry that value.

Everything starts checked, so the group shows the whole scene. **Untick a value
to hide those objects; tick it to bring them back.** To see just the survey
skeleton, untick everything except `Line Plot`. To hide the point cloud while you
work on the survey, untick `LAZ Layer`. To read only the plan drawings, leave
`Plan` ticked and untick the profiles. The change happens live in the 3D view as
you click.

Two conveniences in the value list: click a value's row to select it, **Shift**-
or **Ctrl**-click to select several, and then ticking one checkbox applies to all
selected rows at once; and right-click a row for **Select All**. Objects that
don't carry the chosen key at all fall into a catch-all value called **Others**,
so nothing ever silently disappears from the filter.

## Drill down: add an AND column

One column filters on one key. To narrow *within* that — one caver's plan scraps,
one trip's leads — add another column with the **+** (plus) button at the top of a
column. A new column appears to its right, and you pick a second key for it.

Columns in the same box are combined with **AND**: an object is shown only if it
passes **every** column. And each new column filters **only the objects that
survived the column to its left** — that's the drill-down. Group the first column
by **Type** and tick only `Plan`, add a second column and group it by **Caver**,
and that second column lists only the cavers who appear among the plan scraps;
tick one and you're looking at exactly that person's plan passage. Add a third
column on **Year** to narrow it to one season. Each column drills one level
deeper into what the previous columns left standing.

![A filter group with two columns side by side: the left column grouped by Type with only Plan ticked, the right column grouped by Caver listing the surveyors among those plan scraps with one ticked — the two columns combining with AND to show one caver's plan passage.](../images/view-3d-layers-drilldown.png)
*Columns in one box combine with AND, and each column filters what the one to its
left kept. Here Type is narrowed to Plan, then Caver narrows those plan scraps to
a single surveyor.*

The **−** (dash) button on a column removes it, widening the filter back out by
one level. A column's values all start ticked, because an AND column's job is to
*narrow* — you subtract what you don't want.

## Combine independent selections: Also Include (OR)

A single box can only ever narrow, because its columns all AND together. But often
you want two things that a single narrowing can't express at once — say one
caver's plan passage **and** every lead in the cave, or two different caves side
by side. That's what **Also Include** is for.

The **Also Include** button at the bottom of the panel adds a second bordered box
below the first. Each box is a filter of its own, evaluated independently against
the whole scene, and the boxes are combined with **OR**: the view shows whatever
the first box keeps, *plus* whatever the second box keeps. Build "one caver's plan
scraps" in the first box, click **Also Include**, and in the new box group by
**Type** and tick `Lead` — now you see that caver's passage together with all the
leads, and nothing else.

One difference to expect: a fresh **Also Include** box starts with **nothing**
ticked, the opposite of the first box. That's deliberate — its whole purpose is to
*add* things back into the view, so you tick the values you want to pull in rather
than untick the ones you don't. Add as many boxes as you need; each one unions its
result into what's shown.

That combination — AND across the columns in a box to narrow, OR across the boxes
to add independent selections — is what lets you express visibility as specific as
"this trip's running profiles, plus every lead, plus the point cloud" without
touching anything else in the cave.

## Where to go next

- **[The 3D View](the-3d-view.md)** — orbiting, aiming the camera, and the
  compass and scale bar that the Layers tab sits beside.
- **[How a Project Is Organized](../concepts/data-model.md)** — why a scrap
  carries its trip's and cave's keywords, so one object answers to many filters.
- **[Track and Export Leads](../leads/track-and-export-leads.md)** — the leads you
  can isolate here with `Type = Lead`.
- **[Add a Point Cloud](../point-clouds/add-a-point-cloud.md)** — hiding and
  showing a point cloud is the same `Type = LAZ Layer` filter.
</content>
</invoke>
