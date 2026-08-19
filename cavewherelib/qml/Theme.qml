import QtQuick
import QtQuick.Controls as QC
import cavewherelib

pragma Singleton

QtObject {
    id: theme

    // Track the OS/application color scheme
    readonly property bool dark: Qt.application.styleHints.colorScheme === Qt.Dark

    // Base palette hook using the active system palette
    readonly property SystemPalette palette: SystemPalette { colorGroup: SystemPalette.Active }

    // Core surfaces/text
    readonly property color background: palette.window
    readonly property color surface: dark ? "#1f232a" : "#ffffff"
    readonly property color surfaceMuted: dark ? "#292d35" : "#f6f6f6"
    readonly property color surfaceRaised: dark ? "#242933" : "#f0f0f0"
    // readonly property color sidebar: dark ? "#141414" : "#f4f4f4"
    readonly property color text: palette.text
    readonly property color textSecondary: dark ? "#cdd2db" : "#33363a"
    readonly property color textSubtle: dark ? "#9fa6b1" : "#616469"
    readonly property color textInverse: dark ? "#111318" : "#f5f5f5"
    readonly property color textLink: dark ? "#85c1f4" : "#1d4d77"
    // Red foreground for an invalid value (e.g. an out-of-domain coordinate cell),
    // legible on the page background in both themes — danger is a fill, not text.
    readonly property color errorText: dark ? "#f47067" : "#cf222e"

    // Accents & states
    readonly property color accent: palette.accent
    readonly property color accentMuted: "#8AC6FF"
    readonly property color success: dark ? "#76e596" : "#4caf50"
    readonly property color warning: dark ? "#6b643e" : "#FF9C14"
    // A warning-toned card: a tinted ground, its outline, and text that reads on
    // it. Used where a warning has to be a legible block rather than a fill.
    readonly property color warningSurface: dark ? "#3a3524" : "#fff3df"
    readonly property color warningBorder: dark ? "#8a8250" : "#e0a64b"
    readonly property color warningText: dark ? "#f0d9a0" : "#7a4b00"
    readonly property color danger: dark ? "#6f312e" : "#FF6736"
    readonly property color info: dark ? "#1f3f61" : "#85c1f4"
    readonly property color highlight: dark ? "#314f78" : "#a5cdff"
    readonly property color hover: Qt.lighter(highlight, dark ? 1.4 : 1.15)
    readonly property color icon: palette.buttonText
    readonly property color tag: dark ? "#656565" : border

    // Lines and outlines
    readonly property color border: dark ? "#4a4f58" : "#d3d3d3"
    readonly property color borderSubtle: dark ? "#353a42" : "#e4e4e4"
    readonly property color divider: dark ? "#2c3138" : "#d8d8d8"

    // Sketch palette. Cave maps are paper-first; dark mode uses tuned
    // light grays rather than a literal color inversion.
    readonly property color sketchGridLine:            dark ? "#3a566c" : "#1eb6dd"
    readonly property color sketchGridLabel:           sketchGridLine
    readonly property color sketchGridLabelBackground: background
    readonly property color sketchStrokeWall:          dark ? "#d4d4d4" : "#202020"
    readonly property color sketchStrokeNonWall:       dark ? "#9a9a9a" : "#606060"
    readonly property color sketchStation:             sketchStrokeWall
    readonly property color sketchShotLine:            sketchStrokeNonWall

    // Diff colors
    readonly property color diffAddedBackground: dark ? "#1a3626" : "#dafbe1"
    readonly property color diffDeletedBackground: dark ? "#3d1f1f" : "#ffebe9"
    readonly property color diffHunkBackground: dark ? "#1c2d4d" : "#ddf4ff"
    readonly property color diffAddedText: dark ? "#76e596" : "#1a7f37"
    readonly property color diffDeletedText: dark ? "#f47067" : "#cf222e"
    readonly property color diffHunkText: dark ? "#85c1f4" : "#0969da"
    readonly property color diffContextBackground: "transparent"

    // Git graph lane colors (8-entry cycling palette)
    readonly property list<color> laneColors: [
        "#4dc9f6", "#f67019", "#f53794", "#537bc4",
        "#acc236", "#166a8f", "#00a950", "#58595b"
    ]

    // Typography — driven by cwFontSettings; scale all sizes proportionally
    readonly property real fontScale: RootData.settings.fontSettings.fontBaseSize / 16.0
    readonly property string fontFamily: RootData.settings.fontSettings.fontFamily !== ""
        ? RootData.settings.fontSettings.fontFamily
        : RootData.settings.fontSettings.systemFontFamily
    readonly property string fontFamilyBody: RootData.settings.fontSettings.systemFontFamily
    readonly property string fontFamilyMono: "Courier Prime"

    // A fixed display+body pairing for long-form reading (the manual), independent
    // of the user-configurable UI chrome font: condensed Yanone Kaffeesatz heads
    // over a readable Fira Sans body, so the reading typography stays consistent
    // whichever family the chrome uses.
    readonly property string fontFamilyHeading: "Yanone Kaffeesatz"
    readonly property string fontFamilyReading: "Fira Sans"

    readonly property int fontSizeCaption: Math.round(11 * fontScale)
    readonly property int fontSizeSmall:   Math.round(12 * fontScale)
    readonly property int fontSizeBody:    Math.round(14 * fontScale)
    readonly property int fontSizeUI:      RootData.settings.fontSettings.fontBaseSize
    readonly property int fontSizeMedium:  Math.round(18 * fontScale)
    readonly property int fontSizeTitle:   Math.round(20 * fontScale)
    readonly property int fontSizeLarge:   Math.round(24 * fontScale)
    readonly property int fontSizeXLarge:  Math.round(30 * fontScale)

    // Responsive layout tiers
    enum LayoutSize { Narrow, Medium, Wide }

    // Responsive breakpoints (window width in pixels)
    readonly property int breakpointWide: 800
    readonly property int breakpointMedium: 500
    readonly property int breakpointPanelCollapse: 600
    readonly property int breakpointFullGallery: 1200

    // Sidebar dimensions per tier
    readonly property int sidebarWidthFull: 80
    readonly property int sidebarWidthCompact: 50

    // Per-page tool rail: icon-only buttons, sized so two fit across the wide
    // sidebar, grouped inside a card that lifts them off the dark gradient.
    readonly property int toolRailButtonSize: 30
    readonly property int toolRailSpacing: 4
    readonly property int toolRailPanelInset: 3
    readonly property int toolRailPanelPadding: 4

    // Tool property flyout: the sidebar-hinged panel showing the armed tool's
    // options. Sized to hold a compact options card; sits a small gap off the
    // sidebar's right edge.
    readonly property int toolFlyoutWidth: 220
    readonly property int toolFlyoutGap: 8
    readonly property int toolFlyoutPadding: 11

    // Sidebar update footer: the one control at the bottom of the sidebar that
    // shows whichever derived-data state the update coordinator is in.
    readonly property int updateFooterPadding: 5
    readonly property int updateFooterSpacing: 3
    readonly property int updateFooterChevronSize: 12

    // Task progress ring: the one busy mark, shared by the sidebar footer and
    // the phone status chip. The track is the part not yet done, so it has to
    // read as a groove behind the arc rather than as a second arc.
    readonly property color progressRingTrack: dark ? "#3f4652" : "#c9ced6"
    // Smaller than fontSizeCaption: the count sits inside the ring, whose inner
    // opening is only about two thirds of the mark.
    readonly property int progressRingCountFontSize: Math.round(9 * fontScale)

    // Task flyout: the list of running jobs the footer's busy row opens. Wider
    // than the tool flyout because job names carry file names, and capped so a
    // burst of imports scrolls instead of running off the top of the window.
    readonly property int taskFlyoutWidth: 280
    readonly property int taskFlyoutMaxListHeight: 220
    // Long enough for the pointer to cross the gap from the sidebar to the card.
    readonly property int taskFlyoutHoverCloseDelay: 300

    // Icon sizes
    readonly property int iconSizeButton: 16
    readonly property int iconSizeSmall: 24
    readonly property int iconSizeMedium: 32

    // Coordinate-system picker: keep the UTM zone spinbox and N/S combo
    // compact so mode + zone + hemisphere fit one row without overflowing the
    // project panel or a fix-station table cell.
    readonly property int csZoneFieldWidth: 84
    readonly property int csHemisphereFieldWidth: 64
    // Wide enough for the longest datum the table names ("Mexico ITRF2008") at
    // the picker's smaller datum font.
    readonly property int csDatumFieldWidth: 130
    // How long a pointer rests before a tooltip appears.
    readonly property int toolTipDelay: 500
    // Cap the inline Custom resolved-name label so a long CRS name elides
    // instead of stretching the picker past its host cell / wrapping the Flow.
    readonly property int csResolvedLabelMaxWidth: 180
    // The whole-coordinate field in the inline fix-station editor: wide enough
    // for a UTM triple with its elevation unit, e.g.
    // "610016.792, 5615117.075, 2545.34m". Also caps the error line below it, so
    // a long message wraps inside the popup instead of widening it.
    readonly property int fixPopupCoordinateWidth: 260

    // Touch target sizing — scale up hit points on mobile builds
    readonly property real pointSizeFactor: RootData.mobileBuild ? 2.0 : 1.0

    // Spacing
    readonly property int pageMargin: 8
    readonly property int delegatePadding: 4
    readonly property int tightSpacing: 2
    readonly property int flowSpacing: 6
    readonly property int sectionSpacing: 8
    readonly property int columnGap: 12
    readonly property int actionBarSpacing: 16
    readonly property int statsPadding: 10
    readonly property int floatingToolbarPadding: 12
    readonly property int infoColumnMaxWidth: 200
    // Comfortable width for an inline banner that floats over a page: wide
    // enough to read a sentence or two without crowding the page behind it.
    readonly property int inlineBannerWidth: 460
    // Room a fixed-width table sets aside for a vertical scrollbar when it works
    // out whether it still fits the page it is on.
    readonly property int scrollBarAllowance: 16

    // Utility
    readonly property color transparent: "#00000000"
    readonly property color shadow: dark ? "#33000000" : "#22000000"
    readonly property color focusRing: dark ? "#b0d3ff" : "#5a9bff"

    // Legacy values mapped from the previous Theme.js
    readonly property color floatingWidgetColor: dark ? "#2b3038" : "#DDDDDD"
    readonly property color floatingWidgetRaisedColor: dark
        ? Qt.lighter(floatingWidgetColor, 1.3)
        : Qt.darker(floatingWidgetColor, 1.12)
    readonly property real floatingWidgetRadius: 3
    readonly property color errorBackground: danger

    // View (3D scene) radial background gradient + grid lines
    readonly property QtObject viewBackground: QtObject {
        readonly property color gradientInner: dark ? "#1a2a3d" : "#92D7F8"
        readonly property color gradientOuter: dark ? "#0b0e13" : "#F3F8FB"
        readonly property color gridLineColor: dark ? "#585a5e" : "#000000"
    }

    // Sidebar-specific palette (original colors retained)
    readonly property QtObject sidebar: QtObject {
        readonly property color background: palette.window
        readonly property color gradientTop: "#1b2331"
        readonly property color gradientBottom: dark ? theme.surface : "#616469"
        readonly property color panel: palette.window
        readonly property color divider: theme.divider //"#141414"
        readonly property color hoverStart: "#00d1d1d1"
        readonly property color hoverMid: "#96b5b5b5"
        readonly property color hoverMidHover: "#32b5b5b5"
        readonly property color toggledStart: "#ffffff"
        readonly property color toggledMid: "#000000"
        readonly property color toggledEnd: "#c8c0c0c0"
        readonly property color text: "#ffffff"
        readonly property color textActive: "#000000"
        readonly property color textStroke: "#aaaaaa"
        readonly property color borderActive: "#313131"
        readonly property color borderHover: "#ffffff"
    }
}
