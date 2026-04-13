# Responsive Main Layout Plan

## Context

CaveWhere is being ported to iOS but the layout is not mobile-friendly. Currently, desktop vs mobile layout is controlled entirely by a compile-time flag (`RootData.desktopBuild`). We need dynamic, dimension-based layout switching so the UI adapts when the window is resized — testable on desktop by resizing the window, and automatically correct on phones/tablets.

## Approach

Use width breakpoints on `MainContent.qml` to drive three layout tiers. Replace `RootData.desktopBuild` layout gating with `layoutSize` checks. SyncButton is always visible in the link bar. Sidebar nav collapses into a dropdown menu at narrow widths.

## Breakpoints (added to Theme.qml)

| Tier | Width | Sidebar | LinkBar |
|------|-------|---------|---------|
| Wide | >= 800px | Full 80px sidebar | Back/fwd + breadcrumb + Sync + Discord |
| Medium | 500-799px | Compact ~50px (icons only) | Back/fwd + breadcrumb + Sync (no Discord) |
| Narrow | < 500px | Hidden -- dropdown menu from hamburger button | Hamburger + back/fwd + breadcrumb + Sync |

## File Changes

### 1. `cavewherelib/qml/Theme.qml` -- Add layout enum, breakpoints, and sidebar tokens

```qml
// Responsive layout tiers
enum LayoutSize { Narrow, Medium, Wide }

// Responsive breakpoints (window width in pixels)
readonly property int breakpointWide: 800
readonly property int breakpointMedium: 500

// Sidebar dimensions per tier
readonly property int sidebarWidthFull: 80
readonly property int sidebarWidthCompact: 50
```

The enum is used as `Theme.LayoutSize.Wide`, `Theme.LayoutSize.Medium`, `Theme.LayoutSize.Narrow` throughout QML. The integer ordering (Narrow=0, Medium=1, Wide=2) allows `>=` comparisons like `layoutSize >= Theme.LayoutSize.Medium`.

### 2. `cavewherelib/qml/MainContent.qml` -- Layout orchestration

- Add `layoutSize` property based on `width` vs breakpoints
- Replace `visible: RootData.desktopBuild` on sidebar with `visible: layoutSize >= Theme.LayoutSize.Medium`
- Replace container anchor: `anchors.left: layoutSize >= Theme.LayoutSize.Medium ? mainSideBar.right : parent.left`
- Pass `layoutSize` to LinkBar and MainSideBar
- Keep the `Component.onCompleted` initial page selection gated on `RootData.desktopBuild` (that's a one-time startup decision, not a layout concern)

```qml
readonly property int layoutSize: {
    if (width >= Theme.breakpointWide) return Theme.LayoutSize.Wide
    if (width >= Theme.breakpointMedium) return Theme.LayoutSize.Medium
    return Theme.LayoutSize.Narrow
}
```

**Note on sidebar visibility + anchoring:** When `layoutSize` drops from Medium to Narrow, the sidebar goes `visible: false` and the container anchor switches from `mainSideBar.right` to `parent.left` simultaneously. This is an instant switch -- no animation. If sidebar width animations are added in the future, the anchor switch would need to be deferred until the animation completes (e.g., animate width to 0 first, then switch the anchor). For now the instant switch is correct.

### 3. `cavewherelib/qml/MainSideBar.qml` -- Responsive width

- Accept `property int layoutSize` from parent
- Change `width: 80` to `width: layoutSize >= Theme.LayoutSize.Wide ? Theme.sidebarWidthFull : Theme.sidebarWidthCompact`
- At compact (50px): hide text labels on SideBarButtons, show icons only
- File menu button: change `active: RootData.desktopBuild && !isMacOS` to `active: layoutSize >= Theme.LayoutSize.Wide && !isMacOS` (file menu only in wide mode)
- **TaskListView at compact width:** The task name label already uses `elide: Text.ElideRight` and the progress bar uses `Layout.fillWidth`, so these scale down to 50px acceptably. No changes needed.
- **Auto-update checkbox at compact width:** At 50px the "Automatic\nUpdate" label is too wide. In compact mode: hide the label text, keep only the checkbox centered. The checkbox alone is self-explanatory in the sidebar context.

### 4. `cavewherelib/qml/SideBarButton.qml` -- Support icon-only mode

- Add `property bool compactMode: false`
- When `compactMode`: hide text label, reduce icon size, shrink height
- MainSideBar passes `compactMode: layoutSize === Theme.LayoutSize.Medium` to each button

### 5. `cavewherelib/qml/LinkBar.qml` -- Major restructure

**Goal:** SyncButton always visible at all widths. Back/forward always visible. Hamburger menu at narrow widths. DiscordChatButton hidden at medium and narrow.

Changes:
- Add `required property int layoutSize`
- **Remove the `DesktopLoader` wrapper** around SyncButton/DiscordChatButton
- Move SyncButton (and all its signal handlers, popups, connections) to be a direct always-present child
- DiscordChatButton: `visible: layoutSize >= Theme.LayoutSize.Wide` (wide only)

**Hamburger menu page wiring:** The hamburger menu needs access to the same page addresses as the sidebar. Use `RootData.pageSelectionModel.gotoPageByName(null, "View")` / `"Data"` / `"Map"` for navigation. This uses the string-based page lookup that's already registered in `MainContent.qml:177-181`, avoiding the need to pass Page object references to LinkBar. The sidebar's `findPage()` history-search behavior (returning the last-visited sub-page) is a nice-to-have that can be added later.

**Narrow layout (layoutSize === Theme.LayoutSize.Narrow):**
- Back/forward buttons remain visible (always present at all sizes)
- Add a hamburger `RoundButton` (list icon) that opens a `QC.Menu` dropdown with View/Data/Map items
- Menu items call `RootData.pageSelectionModel.gotoPageByName(null, name)`
- Layout: `[hamburger] [<-] [->] [breadcrumb] [sync]`

**Medium layout (layoutSize === Theme.LayoutSize.Medium):**
- Back/forward area sized to compact sidebar width
- Layout: `[<- ->] [breadcrumb] [sync]`

**Wide layout (layoutSize === Theme.LayoutSize.Wide):**
- Same as current desktop: `[<- ->] [breadcrumb] [sync] [discord]`

**`sidebarWidth` property:** Keep as `required property int sidebarWidth`. MainContent computes and passes it:
```qml
sidebarWidth: layoutSize >= Theme.LayoutSize.Medium ? mainSideBar.width - 1 : 0
```
At narrow width (layoutSize Narrow), `sidebarWidth` is 0 so the back/forward area uses its own implicit size rather than being constrained to the sidebar-aligned container.

### 6. `cavewherelib/qml/DesktopLoader.qml` -- Delete

Confirmed only used in `LinkBar.qml`. Delete after LinkBar changes. Also remove from `CMakeLists.txt` QML module file list if listed there.

## Implementation Order

1. **Theme.qml** -- Add tokens (zero risk, additive)
2. **SideBarButton.qml** -- Add `compactMode` property
3. **MainSideBar.qml** -- Responsive width, pass compactMode, compact auto-update
4. **MainContent.qml** -- Add `layoutSize`, replace `desktopBuild` checks, pass to children
5. **LinkBar.qml** -- Extract SyncButton from DesktopLoader, add hamburger menu, layout tiers
6. **Cleanup** -- Delete `DesktopLoader.qml`, remove from CMakeLists.txt

## Verification

1. Run the desktop app, resize window through all three breakpoints (>800, 500-800, <500)
2. Verify sidebar transitions: full -> compact (icons only) -> hidden
3. Verify SyncButton is always visible in link bar at all widths
4. Verify back/forward buttons are always visible at all widths
5. Verify hamburger dropdown menu appears at narrow width with View/Data/Map options
6. Verify page navigation works from both sidebar and dropdown
7. Verify DiscordChatButton hides at medium and narrow
8. Verify auto-update checkbox is visible (label hidden) at compact sidebar width
9. Test on iOS simulator at phone and tablet sizes
