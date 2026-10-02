import QtQuick
import cavewherelib

pragma Singleton

QtObject {
    id: theme

    // Track the color scheme: the Appearance setting first, the OS second
    readonly property bool dark: RootData.settings.appearanceSettings.dark

    // Shows the system palette on the Colors page. Tokens below are fixed
    // values; none of them reads this object.
    readonly property SystemPalette palette: SystemPalette { colorGroup: SystemPalette.Active }

    // Core surfaces/text
    readonly property color background: dark ? "#313131" : "#F2F2F2"
    readonly property color surface: dark ? "#3A3A3A" : "#FFFFFF"
    readonly property color surfaceMuted: dark ? "#2B2B2B" : "#E9E9E9"
    readonly property color surfaceRaised: dark ? "#454545" : "#FFFFFF"
    // readonly property color sidebar: dark ? "#141414" : "#f4f4f4"
    readonly property color text: dark ? "#FFFFFF" : "#1E1E1E"
    readonly property color textSecondary: dark ? "#C9C9C9" : "#555555"
    readonly property color textSubtle: dark ? "#A8A8A8" : "#6A6A6A"
    readonly property color textInverse: dark ? "#232323" : "#FFFFFF"
    readonly property color textLink: dark ? "#38BDD9" : "#086A86"
    readonly property color textDisabled: Qt.rgba(text.r, text.g, text.b, disabledOpacity)
    // Red foreground for an invalid value (e.g. an out-of-domain coordinate cell)
    // or a destructive button's label, legible on the page and on a button face
    // in both themes — danger is a fill, not text.
    readonly property color errorText: dark ? "#f47067" : "#cf222e"

    // Accents & states
    readonly property color accent: dark ? "#38BDD9" : "#1493B3"
    readonly property color accentMuted: dark ? "#2A8FA6" : "#8FD3E4"
    readonly property color success: dark ? "#76e596" : "#4caf50"
    readonly property color warning: dark ? "#6b643e" : "#FF9C14"
    // A warning-toned card: a tinted ground, its outline, and text that reads on
    // it. Used where a warning has to be a legible block rather than a fill.
    readonly property color warningSurface: dark ? "#3a3524" : "#fff3df"
    readonly property color warningBorder: dark ? "#8a8250" : "#e0a64b"
    readonly property color warningText: dark ? "#f0d9a0" : "#7a4b00"
    readonly property color danger: dark ? "#6f312e" : "#FF6736"
    readonly property color info: dark ? "#12405F" : "#D1F5FA"
    readonly property color highlight: dark ? "#1E5261" : "#C5E6EF"
    readonly property color hover: dark ? "#424242" : "#E4E4E4"
    readonly property color icon: text
    readonly property color tag: dark ? "#656565" : border

    // Splays: wall shots that hang off a station instead of joining the
    // centerline. An earth tone keeps them apart from the survey data they sit
    // next to without reading as an error.
    readonly property color splaySurface: dark ? "#322a1a" : "#f7f1e3"
    readonly property color splayBorder: dark ? "#c9a35e" : "#8a6d3b"
    readonly property color splayText: dark ? "#c9a35e" : "#8a6d3b"

    // How far back the blank row at the bottom of an open splay cluster sits
    // while it waits for a reading. Qt Quick draws no dashed border without
    // Shapes, which the survey table stays clear of, so it fades instead.
    readonly property real splayWaitingOpacity: 0.55

    // The round "+" a station with no splays offers while the pointer is over
    // its Splays cell. It reads as a button in a 50pt station row, where the
    // small-sized glyph it replaced read as a stray character. The bars that
    // draw the "+" span a little under half the button, leaving a ring of the
    // button's own fill around them.
    readonly property int splayEntryButtonSize: 28
    readonly property int splayEntryGlyphThickness: 2
    readonly property real splayEntryGlyphSpan: 0.45

    // Lines and outlines
    readonly property color border: dark ? "#4C4C4C" : "#D2D2D2"
    readonly property color borderSubtle: dark ? "#424242" : "#E0E0E0"
    readonly property color divider: dark ? "#454545" : "#D8D8D8"

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
    // A page button other than the current one, until the pointer reaches it
    readonly property real sidebarIdleOpacity: 0.5
    // Room above and below the link bar, so its rounded outline clears the
    // window edge and the divider under the bar
    readonly property int linkBarVerticalMargin: 4

    // Per-page tool rail: icon-only buttons, sized so two fit across the wide
    // sidebar, grouped inside a card that lifts them off the sidebar.
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
    readonly property color progressRingTrack: track
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
    // A hairline: the detail line under a task row is a secondary signal, so it
    // reads as a thinner mark than the row's own bar.
    readonly property int taskDetailBarHeight: 4

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
    readonly property color shadow: dark ? "#6B000000" : "#29000000"
    readonly property color focusRing: dark ? "#38BDD9" : "#0A7391"

    // ---- BEGIN style tokens (direction D, Karst Soft) ----
    // Surfaces
    readonly property color canvas: dark ? "#565656" : "#C9CBCD"
    readonly property color chrome: dark ? "#232323" : "#DADCDE"
    readonly property color chromeText: dark ? "#FFFFFF" : "#1E1E1E"
    readonly property color rowAlternate: dark ? "#3A3A3A" : "#E7E7E7"

    // Buttons
    readonly property color buttonSurface: dark ? "#454545" : "#FFFFFF"
    readonly property color buttonBorder: dark ? "#606060" : "#C2C2C2"
    readonly property color buttonHover: dark ? "#505050" : "#F0F0F0"
    readonly property color buttonHoverBorder: dark ? "#767676" : "#A8A8A8"
    readonly property color buttonPressed: dark ? "#2A2A2A" : "#DEDEDE"
    readonly property color buttonPressedBorder: dark ? "#606060" : "#A8A8A8"
    readonly property color buttonChecked: dark ? "#626262" : "#D6D6D6"
    readonly property color buttonCheckedBorder: dark ? "#A6A6A6" : "#858585"
    readonly property color buttonPrimary: dark ? "#FFFFFF" : "#2B2B2B"
    readonly property color buttonPrimaryHover: dark ? "#DADADA" : "#4A4A4A"
    readonly property color buttonPrimaryText: dark ? "#232323" : "#FFFFFF"
    readonly property color buttonShadow: dark ? "#4D000000" : "#14000000"

    // Fields: TextField, TextArea, ComboBox, SpinBox
    readonly property color fieldSurface: dark ? "#262626" : "#FFFFFF"
    readonly property color fieldBorder: dark ? "#606060" : "#C2C2C2"
    readonly property color fieldPlaceholder: dark ? "#9A9A9A" : "#8A8A8A"
    readonly property color hoverOverlay: dark ? "#14FFFFFF" : "#12000000"

    // Indicators: CheckBox, RadioButton, Switch, Slider
    readonly property color controlBorder: dark ? "#939393" : "#858585"
    readonly property color checkFill: dark ? "#FFFFFF" : "#2B2B2B"
    readonly property color checkMark: dark ? "#232323" : "#FFFFFF"
    readonly property color track: dark ? "#565656" : "#C4C4C4"
    readonly property color scrollHandle: dark ? "#9E9E9E" : "#8C8C8C"

    // Popups: Menu, ComboBox list, Dialog, ToolTip
    readonly property color popupSurface: dark ? "#3A3A3A" : "#FFFFFF"
    readonly property color popupBorder: dark ? "#606060" : "#C2C2C2"
    readonly property color popupSelected: dark ? "#565656" : "#E4E4E4"
    readonly property color popupShadow: dark ? "#6B000000" : "#29000000"
    // The palette's shadow role, which the fallback style draws its edges with.
    readonly property color paletteShadow: "#000000"
    readonly property color toolTipSurface: dark ? "#FFFFFF" : "#2B2B2B"
    readonly property color toolTipText: dark ? "#232323" : "#FFFFFF"
    readonly property color overlayScrim: dark ? "#66000000" : "#40000000" // behind a modal popup

    // Tabs
    readonly property color tabStrip: dark ? "#232323" : "#DADCDE"
    readonly property color tabText: dark ? "#A8A8A8" : "#6A6A6A"

    // Progress sweep: ProgressBar and BusyIndicator
    readonly property color progressStart: dark ? "#70EBC9" : "#03A899"
    readonly property color progressMid: dark ? "#2EE8E3" : "#2EC4D6"
    readonly property color progressEnd: dark ? "#38BDD9" : "#4096CF"
    readonly property color progressLead: dark ? "#FFFFFF" : "#0A3659"

    // Link bar
    readonly property color linkBar: dark ? "#262626" : "#FFFFFF"
    readonly property color linkBarChip: dark ? "#454545" : "#E4E4E4"

    // Metrics, in logical pixels
    readonly property int controlRadius: 6
    readonly property int panelRadius: 8
    readonly property int indicatorRadius: 4
    readonly property int controlHeight: 28
    readonly property int controlVerticalPadding: 4
    readonly property int buttonHorizontalPadding: 14
    // Tool and round buttons on every side; tab buttons vertically.
    readonly property int compactButtonPadding: 6
    readonly property int fieldHorizontalPadding: 10
    readonly property int textAreaVerticalPadding: 7
    // Implicit widths: a text field, and the narrower combo and spin boxes.
    readonly property int fieldWidth: 160
    readonly property int compactFieldWidth: 120
    // The chevron column at the right of a spin box, and its glyphs.
    readonly property int spinIndicatorWidth: 18
    readonly property int spinChevronSize: 8
    // The chevron on a combo box and beside a submenu.
    readonly property int chevronSize: 12
    // A row in a list or menu: its implicit height and highlight corners.
    readonly property int listRowHeight: 24
    readonly property int rowRadius: 4
    readonly property int controlSpacing: 6
    readonly property int indicatorSize: 16
    // The check and dash glyph inside a check box.
    readonly property int indicatorGlyphSize: 14
    readonly property int radioDotSize: 6
    readonly property int switchWidth: 34
    readonly property int switchHeight: 18
    // Gap between the switch's pill edge and its thumb.
    readonly property int switchThumbInset: 2
    readonly property int sliderHandleSize: 16
    readonly property int sliderTrackHeight: 2
    // A slider's implicit size: length along the groove, thickness across it.
    readonly property int sliderLength: 150
    readonly property int sliderThickness: 18
    // The switch thumb's slide.
    readonly property int toggleAnimationDuration: 150
    readonly property int progressBarHeight: 5
    readonly property int progressBarWidth: 150
    // Where the mid and end colors sit along the progress sweep, 0 to 1.
    readonly property real progressMidPosition: 0.62
    readonly property real progressEndPosition: 0.86
    // An indeterminate bar: its share of the track and one pass across it.
    readonly property real progressIndeterminateFraction: 0.35
    readonly property int progressSlideDuration: 1600
    // The busy indicator: its implicit size, one turn of the ring, and its
    // fade in and out.
    readonly property int busyIndicatorSize: 44
    readonly property int busySpinDuration: 1100
    readonly property int busyFadeDuration: 150
    // The droplet opening and the expanding close play at this size and above.
    readonly property int busyDropletMinimumSize: 32
    // Opening: a drop falls to the center, two ripples spread from where it
    // lands, and the ring grows in from a smaller scale.
    readonly property int busyDropSize: 4
    readonly property int busyDropFallDuration: 450
    readonly property real busyRippleBorderWidth: 1.5
    readonly property real busyRippleStartScale: 0.1
    readonly property real busyRippleStartOpacity: 0.9
    readonly property int busyRippleDuration: 700
    readonly property int busyRippleStagger: 150
    readonly property real busyRingEnterScale: 0.6
    readonly property int busyRingEnterDuration: 350
    // Closing: the ring expands as it fades out.
    readonly property real busyRingExitScale: 1.45
    readonly property int busyRingExitDuration: 500
    // Group boxes and frames: the padding around their content.
    readonly property int containerPadding: 9
    readonly property int groupBoxHorizontalPadding: 11
    // Between a group box title and the content below it.
    readonly property int groupBoxTitleSpacing: 5
    // A split view handle, and the wider strip that grabs it.
    readonly property int splitHandleThickness: 5
    readonly property int splitHandleGrabThickness: 11
    readonly property int scrollBarThickness: 8
    readonly property int scrollBarPadding: 2
    readonly property int scrollBarMinimumLength: 24
    readonly property int scrollIndicatorThickness: 4
    readonly property real scrollHandleOpacity: 0.55
    readonly property real scrollHandleHoverOpacity: 0.8
    // A scroll indicator stays visible this long after scrolling stops, then fades.
    readonly property int scrollFadeDelay: 450
    readonly property int scrollFadeDuration: 200
    readonly property int focusRingWidth: 2
    readonly property int focusRingOffset: 2
    readonly property int menuItemHorizontalPadding: 12
    readonly property int menuItemVerticalPadding: 3
    readonly property int menuIndicatorColumn: 14
    // A menu's side padding keeps its rows inside the surface's border.
    readonly property int menuHorizontalPadding: 1
    readonly property int menuMinimumWidth: 100
    // How far a submenu slides back over the menu that opened it.
    readonly property int menuOverlap: 2
    readonly property int menuBarHeight: 24
    readonly property int menuBarItemHorizontalPadding: 10
    readonly property int popupPadding: 3
    // Space between a combo box and the list it opens.
    readonly property int popupGap: 2
    // Around the content of a plain popup, a dialog, and its button row.
    readonly property int dialogPadding: 12
    readonly property int dialogButtonSpacing: 8
    readonly property int toolTipVerticalPadding: 5
    readonly property int toolTipHorizontalPadding: 9
    // Closest a tooltip comes to the window edge, and its gap above its parent.
    readonly property int toolTipMargin: 6
    readonly property int toolTipGap: 4
    // A drawer's slide, in drawer lengths per second.
    readonly property real drawerSlideVelocity: 5
    readonly property int popupShadowBlur: dark ? 14 : 8
    readonly property int popupShadowOffset: dark ? 4 : 2
    // Room around a popup's surface for its shadow. Popups set all four
    // insets to minus this value; StylePopupPanel draws inside it.
    readonly property int popupShadowMargin: 18
    readonly property real disabledOpacity: 0.4
    readonly property real uncheckedToggleOpacity: 0.7
    // ---- END style tokens ----

    // Legacy values mapped from the previous Theme.js
    readonly property color floatingWidgetColor: dark ? "#3A3A3A" : "#FFFFFF"
    readonly property color floatingWidgetRaisedColor: dark
        ? Qt.lighter(floatingWidgetColor, 1.3)
        : Qt.darker(floatingWidgetColor, 1.12)
    readonly property real floatingWidgetRadius: controlRadius
    readonly property color errorBackground: danger

    // View (3D scene) radial background gradient + grid lines
    readonly property QtObject viewBackground: QtObject {
        readonly property color gradientInner: dark ? "#1a2a3d" : "#92D7F8"
        readonly property color gradientOuter: dark ? "#0b0e13" : "#F3F8FB"
        readonly property color gridLineColor: dark ? "#585a5e" : "#000000"
    }

    // Sidebar: a flat panel in the page color with a hairline right edge
    readonly property QtObject sidebar: QtObject {
        readonly property color background: theme.background
        readonly property color panel: theme.surface
        readonly property color divider: theme.border
        readonly property color text: theme.text
    }
}
